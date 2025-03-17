#pragma once
#include <Arduino.h>
#include <WString.h>
#include <host/usbh.h>
#include <tusb.h>
#include <arduino/cdc/Adafruit_USBH_CDC.h>
#include <class/hid/hid.h>
#include <boards/adafruit_feather_rp2040.h>

#include "Serial.hpp"

class SerialUsbHost : public ISerial {
    Adafruit_USBH_CDC* _host;
public:
    SerialUsbHost(Adafruit_USBH_CDC* host) : _host(host) {};
    virtual void begin(long baudrate);
    virtual bool connected();
    virtual int available();
    virtual char read();
    virtual int read(char* buf, int bytes);
    virtual size_t write(const char* buf, int bytes = -1);
    virtual void flush();
};
