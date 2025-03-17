// USBSerialInputDevice.hpp
#pragma once
#include <Arduino.h>
#include <WString.h>

#include "InputDevice.hpp"
#include "USBSerialHost.hpp"

class USBSerialInputDevice : public IInputDevice {
private:
    USBSerialHost _usbHost;
    
public:
    USBSerialInputDevice() {}
    
    virtual void timeSlice() override {
        _usbHost.timeSlice();
    }
    
    virtual Capabilities supportedCapabilities() override {
        return _usbHost.supportedCapabilities();
    }
    
    virtual bool isKeyPressed(KeyPress& pressed) override {
        return _usbHost.isKeyPressed(pressed);
    }
    
    virtual float rawY() override {
        return _usbHost.rawY();
    }
    
    virtual float rawX() override {
        return _usbHost.rawX();
    }
    
    // Pass-through methods for serial access
    bool isConnected() { return _usbHost.isConnected(); }
    void write(const uint8_t* buffer, size_t size) { _usbHost.write(buffer, size); }
    void write(const char* str) { _usbHost.write(str); }
    int available() { return _usbHost.available(); }
    String readString() { return _usbHost.readString(); }
    
    // Get the underlying USBSerialHost
    USBSerialHost* getHost() { return &_usbHost; }
};