#pragma once
#include <Arduino.h>
#include <WString.h>

#include "Controller.hpp"

// Keeps the idle status screen fresh with minimal mount traffic.
//
// Two cadences, selected by client presence: while a PC client is sending
// commands (activity within CLIENT_SILENT_MS), piggyback on its :GX#
// pass-throughs and only send our own poll when none has been seen for
// CLIENT_ACTIVE_INTERVAL_MS. Once the client has been silent for
// CLIENT_SILENT_MS ("standalone mode"), poll every STANDALONE_INTERVAL_MS.
// Own requests are additionally throttled to one per interval in both modes.
class StatusRefresher
{
public:
    static const unsigned long CLIENT_SILENT_MS          = 5000;
    static const unsigned long CLIENT_ACTIVE_INTERVAL_MS = 2000;
    static const unsigned long STANDALONE_INTERVAL_MS    = 1000;

    explicit StatusRefresher(Controller* controller)
    {
        _controller = controller;
    }

    // Call whenever the PC client sent us a command.
    void noteClientActivity(unsigned long now)
    {
        _lastClientActivity = now;
    }

    // Returns true exactly when our own :GX# job should be enqueued now.
    bool shouldRequest(unsigned long now)
    {
        const unsigned long interval = (now - _lastClientActivity >= CLIENT_SILENT_MS)
                                           ? STANDALONE_INTERVAL_MS
                                           : CLIENT_ACTIVE_INTERVAL_MS;

        if (now - _lastOwnRequest < interval)
        {
            return false;
        }
        if (now - _controller->getLastGXTime() < interval)
        {
            return false;
        }
        _lastOwnRequest = now;
        return true;
    }

private:
    Controller* _controller;
    unsigned long _lastClientActivity = 0;
    unsigned long _lastOwnRequest     = 0;
};
