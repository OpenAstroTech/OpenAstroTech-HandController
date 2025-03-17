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

// Handle the command replies coming from the mount
String processSerialFromMount(MessageJob* activeJob, ISerial* serialPort);