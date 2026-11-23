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
    String _lastXGM;
    String _commandReply;
    ControllerState _status;
    IInputDevice* _inputDevice;

public:
    Controller(IInputDevice* inputDevice);
    MessageJob* timeslice();
    void setReply(String reply);
    void setLastGX(String status, long timestamp);
    void setLastXGM(String hardwareInfo);

    String getReply() const { return _commandReply; }
    String getLastGX() const { return _lastGX; }
    long getLastGXTime() const { return _lastGXTime; }
    // The mount's :XGM# hardware profile, cached for the menu's hardware
    // gating (MenuDefinition). Empty until the first reply.
    String getLastXGM() const { return _lastXGM; }
};
