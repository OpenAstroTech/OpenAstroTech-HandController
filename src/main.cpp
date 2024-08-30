#include <Arduino.h>
#include <WString.h>
#include <SoftwareSerial.h>

#include "MessageJob.hpp"
#include "Controller.hpp"
#include "Joystick.hpp"
#include "Serial.hpp"

#define SERIAL_BAUDRATE 19200
#define D5 14
#define D6 12

enum AppState
{
    AppIdle,
    AwaitingCommandReply,
};

AppState currentState;
JobQueue jobQueue;
MessageJob* activeJob = nullptr;
SoftwareSerial* serial2;
IInputDevice* device= new Joystick(15,16);
Controller controller(device);

void setup()
{
    currentState = AppState::AppIdle;
    serial2 = new SoftwareSerial(D5, D6);
    Serial.begin(SERIAL_BAUDRATE);
    serial2->begin(SERIAL_BAUDRATE);
    // pinMode(LED_BUILTIN, OUTPUT);
}

void loop()
{
    // Check if the connected client (ASCOM/OATControl/NINA) has a job for us
    MessageJob* job = processSerialFromClient();
    if (job != nullptr)
    {
        // digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
        LOG(DEBUG_JOBS, "Received Job: [%s] from client", job->getCommand().c_str());
        // Yes, so put it in the queue
        jobQueue.enqueue(job);
    }

    // Check if the controller has a job for us
    job = controller.timeslice();
    if (job != nullptr)
    {
        // digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
        LOG(DEBUG_JOBS, "Received Job: [%s] from controller", job->getCommand().c_str());
        // Yes, so put it in the queue as well
        jobQueue.enqueue(job);
    }

    // See what we're currently doing.
    switch (currentState)
    {
        case AppState::AppIdle:
            {
                // Currently no job is being processed, so check if one is pending and if so, dequeue, send it and await a reply if needed.
                // ASSERT(activeJob == nullptr);
                if (jobQueue.hasJob())
                {
                    activeJob = jobQueue.dequeue();
                    LOG(DEBUG_JOBS, "[Idle] Job [%s] dequeued and processing", activeJob->getCommand().c_str());
                    serial2->print(activeJob->getCommand());
                    if (activeJob->getCommandType() != CommandType::NoReply)
                    {
                        LOG(DEBUG_JOBS, "[Idle] Job requires reply.");
                        currentState = AppState::AwaitingCommandReply;
                    }
                    else
                    {
                        LOG(DEBUG_JOBS, "[Idle] Job does NOT require reply.");
                        // Stay in Idle mode to retrieve next job
                        delete activeJob;
                        activeJob = nullptr;
                        // digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
                    }
                }
            }
            break;

        case AppState::AwaitingCommandReply:
            {
                // Command was sent, we are awaiting a reply from the mount.
                //ASSERT(activeJob!==nullptr);
                String reply = processSerialFromMount(activeJob, serial2);
                if (!reply.isEmpty())
                {
                    if (activeJob->getSource() == JobSource::FromClient)
                    {
                        LOG(DEBUG_JOBS, "[AwaitReply] Received reply [%s], sending to client.", reply.c_str());
                        // Send back to client
                        if (activeJob->getCommand() == "GX")
                        {
                            // Let controller know the last state (since it's free here)
                            controller.setLastGX(reply, millis());
                        }
                        Serial.print(reply);
                    }
                    else if (activeJob->getSource() == JobSource::FromController)
                    {
                        LOG(DEBUG_JOBS, "[AwaitReply] Received reply [%s], sending to controller.", reply.c_str());
                        // Send to controller
                        controller.setReply(reply);
                    }
                    currentState = AppState::AppIdle;
                    delete activeJob;
                    activeJob = nullptr;
                    // digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
                }
            }
            break;
    }
}
