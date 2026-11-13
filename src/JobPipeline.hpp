#pragma once
#include <Arduino.h>
#include <WString.h>

#include "MessageJob.hpp"
#include "Serial.hpp"
#include "Controller.hpp"
#include "Utility.hpp"

// Maximum time to wait for the mount to answer a job that expects a reply.
const unsigned long REPLY_TIMEOUT_MS = 2000;

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
                            _currentState = AppState::AwaitingCommandReply;
                        }
                    }
                }
                break;

            case AppState::AwaitingCommandReply:
                {
                    String reply = processSerialFromMount(_activeJob, _mountSerial);
                    if (reply.length() > 0)
                    {
                        if (_activeJob->getSource() == JobSource::FromClient)
                        {
                            LOG(DEBUG_JOBS, "[AwaitReply] Received reply [%s], sending to client.", reply.c_str());
                            // The stored command is the full ":GX#" form.
                            if (_activeJob->getCommand().startsWith(":GX"))
                            {
                                _controller->setLastGX(reply, millis());
                            }
                            Serial.print(reply);
                        }
                        else if (_activeJob->getSource() == JobSource::FromController)
                        {
                            LOG(DEBUG_JOBS, "[AwaitReply] Received reply [%s], sending to controller.", reply.c_str());
                            _controller->setReply(reply);
                        }
                        finishJob(false);
                    }
                    else if (millis() - _awaitSince > REPLY_TIMEOUT_MS)
                    {
                        // Mount silent or disconnected: drop the job so the
                        // pipeline keeps serving later jobs.
                        LOG(DEBUG_JOBS, "[AwaitReply] Timed out waiting for reply to [%s].", _activeJob->getCommand().c_str());
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
};
