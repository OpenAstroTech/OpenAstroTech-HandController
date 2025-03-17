// USBSerialHost.hpp
#pragma once
#include <Arduino.h>
#include <WString.h>
#include <Adafruit_TinyUSB.h>
#include "InputDevice.hpp"

// USB Host CDC (Serial) class
class USBSerialHost : public IInputDevice {
private:
    // TinyUSB Host objects
    Adafruit_USBH_Host usbh;
    bool _hostReady;
    uint8_t _dev_addr;
    uint8_t _instance;
    bool _connected;
    String _serialBuffer;
    KeyPress _currentKey;
    
public:
    USBSerialHost();
    virtual void timeSlice() override;
    virtual Capabilities supportedCapabilities() override;
    virtual bool isKeyPressed(KeyPress& pressed) override;
    virtual float rawY() override;
    virtual float rawX() override;
    
    // Additional methods for serial communication
    bool isConnected() { return _connected; }
    void write(const uint8_t* buffer, size_t size);
    void write(const char* str);
    int available();
    String readString();
    
private:
    static void task_function(void* param);
};