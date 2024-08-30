#pragma once
#include <Arduino.h>
#include <WString.h>

#include "Utility.hpp"
#include "InputDevice.hpp"

enum class State
{
    Idle,
    KeyNorth,
    KeyEast,
    KeySouth,
    KeyWest,
};

const float XAXIS_RANGE = 1024.0;
const float YAXIS_RANGE = 1024.0;
const float KEY_THRESHOLD = 0.25;
const long KEY_REPEAT_INTERVAL = 400;


class Joystick : public IInputDevice {
private:
    static const int SMOOTHEN_WINDOW_SIZE = 16;
    float _xPosHistory[SMOOTHEN_WINDOW_SIZE];
    float _yPosHistory[SMOOTHEN_WINDOW_SIZE];
    float _xPosTotal;
    float _yPosTotal;
    int _readIndex = 0;
    float _xPos;
    float _yPos;
    long _keyDownAt;
    KeyPress _currentKey;
    KeyPress _lastKey;
    int _xAxisPin;
    int _yAxisPin;

public:
    Joystick(int xAxisPin, int yAxisPin) {
        for (int i = 0; i < SMOOTHEN_WINDOW_SIZE; i++) {
            _xPosHistory[i] = 0;
            _yPosHistory[i] = 0;
        }
        _xPosTotal = 0;
        _yPosTotal = 0;
        _currentKey = KeyPress::None;
        _xAxisPin = xAxisPin;
        _yAxisPin = yAxisPin;
    };

    virtual void timeSlice() {
        float xPosRaw = (analogRead(_xAxisPin) / XAXIS_RANGE) - 0.5f;
        float yPosRaw = (analogRead(_yAxisPin) / YAXIS_RANGE) - 0.5f;
        _xPosTotal -= _xPosHistory[_readIndex];
        _yPosTotal -= _yPosHistory[_readIndex];
        _xPosHistory[_readIndex] = xPosRaw;
        _yPosHistory[_readIndex] = yPosRaw;
        _xPosTotal += xPosRaw;
        _yPosTotal += yPosRaw;

        _readIndex = (_readIndex + 1) % SMOOTHEN_WINDOW_SIZE;
        _xPos = _xPosTotal / SMOOTHEN_WINDOW_SIZE;
        _yPos = _yPosTotal / SMOOTHEN_WINDOW_SIZE;

        if (_currentKey == KeyPress::None) {
            if (_xPos < KEY_THRESHOLD) {
                _currentKey = KeyPress::Left;
            }
            else if (_xPos > (1.0 - KEY_THRESHOLD)) {
                _currentKey = KeyPress::Right;
            }
            if (_yPos < KEY_THRESHOLD) {
                _currentKey = KeyPress::Up;
            }
            else if (_yPos > (1.0 - KEY_THRESHOLD)) {
                _currentKey = KeyPress::Down;
            }
            // If we changed state, remember when...
            if (_currentKey != KeyPress::None) {
                // If it's the same key as last time, has it been less than the repeat interval?
                if ((_lastKey == _currentKey) && (millis() - _keyDownAt < KEY_REPEAT_INTERVAL)) {
                    // Then don't generate a keypress
                    _currentKey = KeyPress::None;
                }
                else {
                    // It's been longer or it's a different key, so generate it and remember the new time
                    _keyDownAt = millis();
                }
            }
        }
    };

    virtual float rawY() { return _xPos; };
    virtual float rawX() { return _yPos; };

    // Returns true if a key has been pressed since last call.
    virtual bool isKeyPressed(KeyPress& pressed)
    {
        if (_currentKey != KeyPress::None) {
            pressed = _currentKey;
            _lastKey = _currentKey;
            _currentKey = KeyPress::None;
            return true;
        }
        return false;
    }

    virtual Capabilities supportedCapabilities() {
        return Capabilities::KeyPress | Capabilities::RawInput;
    };
};

