#pragma once
#include <Arduino.h>
#include <WString.h>

#include "MessageJob.hpp"

class ISerial {
public:
    virtual void begin(long baudrate) = 0;
    virtual bool connected() = 0;
    virtual int available() = 0;
    virtual char read() = 0;
    virtual int read(char* buf, int bytes) = 0;
    virtual size_t  write(const char* buf, int bytes = -1) = 0;
    virtual void flush() = 0;
};

MessageJob* processSerialFromClient();

// Handle the command replies coming from the mount. anyByte, when given, is
// set as soon as the mount has sent anything for the job.
String processSerialFromMount(MessageJob* activeJob, ISerial* serialPort, bool* anyByte = nullptr);

// Forget a reply that was only partly received (a job abandoned before its
// reply was complete).
void discardPartialReply();