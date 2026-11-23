#pragma once
#include <Arduino.h>
#include <WString.h>

#include "MessageJob.hpp"
#include "Serial.hpp"
#include "Controller.hpp"
#include "Utility.hpp"

// Maximum time to wait for a mount reply once it has started to arrive.
const unsigned long REPLY_TIMEOUT_MS = 2000;

// How long to wait when the mount has not sent a single byte. A mount that
// answers does so within about 120 ms, so a command it does not implement -
// or answers with nothing - must not cost the full timeout.
const unsigned long NO_ANSWER_SILENCE_MS = 300;

// Processes queued MessageJobs against the mount: one job in flight at a
// time; replies are routed back to the job's source (client serial or the
// controller). Extracted from main.cpp's loop() so it can be unit-tested.
class JobPipeline
{
public:
    JobPipeline(ISerial* mountSerial, Controller* controller)
    {
        _mountSerial = mountSerial;
        _controller = controller;
        _currentState = AppState::AppIdle;
        _activeJob = nullptr;
    }

    ~JobPipeline()
    {
        delete _activeJob;
    }

    // One non-blocking iteration of the job pump.
    void tick(JobQueue& jobQueue)
    {
        switch (_currentState)
        {
            case AppState::AppIdle:
                {
                    // Discard any mount bytes that arrived while no job was
                    // awaiting (late replies to timed-out jobs, or
                    // unsolicited output): leaving them in the buffer would
                    // let the next job misread them as its own reply.
                    while (_mountSerial->available() > 0)
                    {
                        char stale = 0;
                        _mountSerial->read(&stale, 1);
                        LOG(DEBUG_MOUNT, "[Idle] Discarding stale mount byte while idle.");
                    }

                    if (jobQueue.hasJob())
                    {
                        _activeJob = jobQueue.dequeue();
                        LOG(DEBUG_JOBS, "[Idle] Processing job: [%s]", _activeJob->getCommand().c_str());
                        _mountSerial->write(_activeJob->getCommand().c_str());
                        if (_activeJob->getCommandType() == CommandType::NoReply)
                        {
                            // Fire-and-forget: no reply will come, so finish
                            // the job immediately instead of wedging the
                            // pipeline waiting for one.
                            finishJob(true);
                        }
                        else
                        {
                            _awaitSince = millis();
                            _replyStarted = false;
                            _currentState = AppState::AwaitingCommandReply;
                        }
                    }
                }
                break;

            case AppState::AwaitingCommandReply:
                {
                    String reply = processSerialFromMount(_activeJob, _mountSerial, &_replyStarted);
                    if (reply.length() > 0)
                    {
                        // Cache status replies for both sources so the idle
                        // status screen stays fresh whether a client polled
                        // or we did (StatusRefresher).
                        if (_activeJob->getCommand().startsWith(":GX"))
                        {
                            _controller->setLastGX(reply, millis());
                        }
                        // Cache the hardware profile for both sources so the
                        // menu's hardware gating (MenuDefinition) rebuilds
                        // when the mount answers.
                        else if (_activeJob->getCommand().startsWith(":XGM"))
                        {
                            _controller->setLastXGM(reply);
                        }

                        if (_activeJob->getSource() == JobSource::FromClient)
                        {
                            LOG(DEBUG_JOBS, "[AwaitReply] Received reply [%s], sending to client.", reply.c_str());
                            Serial.print(reply);
                        }
                        else if (_activeJob->getSource() == JobSource::FromController)
                        {
                            LOG(DEBUG_JOBS, "[AwaitReply] Received reply [%s], sending to controller.", reply.c_str());
                            _controller->setReply(reply);
                        }
                        finishJob(false);
                    }
                    else if (millis() - _awaitSince > (_replyStarted ? REPLY_TIMEOUT_MS : NO_ANSWER_SILENCE_MS))
                    {
                        // Mount silent or disconnected: drop the job so the
                        // pipeline keeps serving later jobs. Whatever arrived
                        // of an unfinished reply is dropped with it.
                        LOG(DEBUG_JOBS, "[AwaitReply] Timed out waiting for reply to [%s].", _activeJob->getCommand().c_str());
                        discardPartialReply();
                        finishJob(true);
                    }
                }
                break;
        }
    }

private:
    enum AppState
    {
        AppIdle,
        AwaitingCommandReply,
    };

    // Completes the active job. An empty reply signals "job finished, no
    // reply" to controller-sourced jobs.
    void finishJob(bool withoutReply)
    {
        if (withoutReply && _activeJob->getSource() == JobSource::FromController)
        {
            _controller->setReply("");
        }
        delete _activeJob;
        _activeJob = nullptr;
        _currentState = AppState::AppIdle;
    }

    ISerial* _mountSerial;
    Controller* _controller;
    AppState _currentState;
    MessageJob* _activeJob;
    unsigned long _awaitSince = 0;
    bool _replyStarted = false;  // the mount has sent at least one byte for the job in flight
};
