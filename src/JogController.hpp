#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <string.h>

// The jog screen's throttle state machine (docs/superpowers/specs/
// 2026-11-22-display-interaction-design.md, "Jog mode"). One dial, two
// axes:
//
// - A rotation detent moves the ACTIVE axis one rung of the speed ladder,
//   through zero into the reverse direction, and emits its speed command
//   immediately (:XSX/:XSY - the mount's manual slew mode applies speeds
//   live, no reply).
// - A short click while either axis is moving stops both axes.
// - A short click while stopped toggles which axis the dial moves.
// - A held press (>= 750 ms) stops both axes and exits jog mode
//   (:XSM0#); the press is consumed so the release afterward cannot be
//   mistaken for another click.
//
// tick() returns one queued command per call and nullptr when drained, so
// the caller enqueues exactly what the mount should receive, in order.
// Commands are NoReply and go through the ordinary job pipeline (spec:
// single-writer pipeline kept).

// The speed ladder in deg/s, slowest first. The first rungs are slow
// enough for framing, the last fast enough to cross the sky.
static const int JOG_LADDER_STEPS = 7;
static const char* const JOG_LADDER_STR[JOG_LADDER_STEPS] = {
    "0.020", "0.050", "0.100", "0.250", "0.500", "1.000", "2.000"};
static const float JOG_LADDER[JOG_LADDER_STEPS] = {
    0.02f, 0.05f, 0.1f, 0.25f, 0.5f, 1.0f, 2.0f};
static const unsigned long JOG_LONG_PRESS_MS = 750;

// The narrow input surface jog needs; main.cpp adapts the rotary encoder
// to it, tests use a fake. Pulse getters consume: true exactly once per
// detent.
class JogInput
{
public:
    virtual ~JogInput() {}
    virtual bool upPulse() = 0;
    virtual bool downPulse() = 0;
    virtual bool selectDown() = 0;
    virtual bool selectUp() = 0;
    virtual unsigned long selectHeldMs(unsigned long now) = 0;
};

class JogController
{
public:
    void begin()
    {
        _active = false;
        _axis = 'R';
        _raRung = 0;
        _decRung = 0;
        _pendingCount = 0;
        _pendingNext = 0;
        _wasDown = false;
        _longPressFired = false;
    }

    // Activation emits nothing - the caller sends :XSM1# itself.
    void setActive(bool active) { _active = active; }
    bool isActive() const { return _active; }

    char activeAxis() const { return _axis; }
    float raSpeed() const { return rungSpeed(_raRung); }
    float decSpeed() const { return rungSpeed(_decRung); }
    bool isMoving() const { return _raRung != 0 || _decRung != 0; }

    const char* tick(JogInput* input, unsigned long now)
    {
        // Commands queued earlier go out before new input is read - also
        // after the exit long-press, which deactivates with its own stop
        // commands still queued.
        if (_pendingNext < _pendingCount)
        {
            return _pending[_pendingNext++];
        }
        _pendingCount = 0;
        _pendingNext = 0;

        if (!_active || input == nullptr)
        {
            return nullptr;
        }

        const bool up = input->upPulse();
        const bool down = input->downPulse();
        if (up || down)
        {
            const int delta = up ? 1 : -1;
            if (_axis == 'R')
            {
                _raRung = step(_raRung, delta);
                queueAxisCommand('X', _raRung);
            }
            else
            {
                _decRung = step(_decRung, delta);
                queueAxisCommand('Y', _decRung);
            }
            return pop();
        }

        const bool downNow = input->selectDown();
        if (downNow && !_wasDown)
        {
            _wasDown = true;
            _longPressFired = false;
        }

        if (_wasDown && input->selectUp())
        {
            _wasDown = false;
            if (_longPressFired)
            {
                // The exit long-press already happened; the release is
                // consumed.
                return nullptr;
            }
            if (isMoving())
            {
                stopBoth();
                return pop();
            }
            _axis = (_axis == 'R') ? 'D' : 'R';
            return nullptr;
        }

        if (_wasDown && !_longPressFired && input->selectHeldMs(now) >= JOG_LONG_PRESS_MS)
        {
            _longPressFired = true;
            stopBoth();
            queue(":XSM0#");
            _active = false;
            return pop();
        }

        return nullptr;
    }

private:
    static int step(int rung, int delta)
    {
        rung += delta;
        if (rung > JOG_LADDER_STEPS)
        {
            rung = JOG_LADDER_STEPS;
        }
        if (rung < -JOG_LADDER_STEPS)
        {
            rung = -JOG_LADDER_STEPS;
        }
        return rung;
    }

    static float rungSpeed(int rung)
    {
        if (rung == 0)
        {
            return 0.0f;
        }
        const int magnitude = rung < 0 ? -rung : rung;
        const float speed = JOG_LADDER[magnitude - 1];
        return rung < 0 ? -speed : speed;
    }

    void queue(const char* cmd)
    {
        if (_pendingCount < 4)
        {
            _pending[_pendingCount++] = cmd;
        }
    }

    void queueAxisCommand(char axis, int rung)
    {
        char buf[16];
        // The mount parses the payload with strtod; the ladder strings are
        // preformatted so no float formatting is involved. Rung 0 (stopped)
        // has no ladder entry.
        if (rung == 0)
        {
            strcpy(buf, "0.000#");
        }
        else
        {
            const int magnitude = rung < 0 ? -rung : rung;
            snprintf(buf, sizeof(buf), "%s%s#", rung < 0 ? "-" : "", JOG_LADDER_STR[magnitude - 1]);
        }
        static char cmdBuf[2][16];  // two axis slots, one command each
        const int slot = (axis == 'X') ? 0 : 1;
        snprintf(cmdBuf[slot], sizeof(cmdBuf[slot]), ":XS%c%s", axis, buf);
        queue(cmdBuf[slot]);
    }

    void stopBoth()
    {
        _raRung = 0;
        _decRung = 0;
        queueAxisCommand('X', 0);
        queueAxisCommand('Y', 0);
    }

    const char* pop()
    {
        const char* cmd = _pending[_pendingNext++];
        if (_pendingNext >= _pendingCount)
        {
            _pendingCount = 0;
            _pendingNext = 0;
        }
        return cmd;
    }

    bool _active;
    char _axis;
    int _raRung;
    int _decRung;
    const char* _pending[4];
    uint8_t _pendingCount;
    uint8_t _pendingNext;
    bool _wasDown;
    bool _longPressFired;
};
