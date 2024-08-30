#include <Arduino.h>
#include <WString.h>

#include "Configuration.hpp"
#include "MessageJob.hpp"
#include "Controller.hpp"

Controller::Controller(IInputDevice* inputDevice)
{
    _inputDevice = inputDevice;
    _status = ControllerState::Idle;
}

MessageJob* Controller::timeslice()
{
    _inputDevice->timeSlice();
    
    MessageJob* job = nullptr;
    switch (_status) {
        case ControllerState::Idle:
            {
                // Send command
                // create job
                // if reply needed:  _status=ControllerState::AwaitingReply;
            }
            break;
        case ControllerState::AwaitingReply:
            {
                if (!_commandReply.isEmpty()) {

                    // Process the reply

                    _commandReply = "";
                    _status = ControllerState::Idle;
                }
            }
            break;
    }
    return job;
}

void Controller::setLastGX(String lastGXReply, long timestamp) {
    _lastGX = lastGXReply;
    _lastGXTime = timestamp;
}

void Controller::setReply(String reply)
{
    _commandReply = reply;
}
