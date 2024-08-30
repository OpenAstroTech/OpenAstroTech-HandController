#pragma once
#include <Arduino.h>
#include <WString.h>

#include "MessageJob.hpp"
#include "InputDevice.hpp"

enum ControllerState
{
    Idle,
    AwaitingReply,
};


class Controller {
    String _lastGX;
    long _lastGXTime;
    String _commandReply;
    ControllerState _status;
    IInputDevice* _inputDevice;

public:
    Controller(IInputDevice* inputDevice);
    MessageJob* timeslice();
    void setReply(String reply);
    void setLastGX(String status, long timestamp);
};
