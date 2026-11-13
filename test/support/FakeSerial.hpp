// ISerial test double: feed() supplies mount-side bytes, written captures
// firmware-side writes.
#pragma once

#include "Serial.hpp"

class FakeSerial : public ISerial
{
public:
    void begin(long baudrate) override { (void) baudrate; }
    bool connected() override { return _connected; }
    int available() override { return (int) (_input.length() - _pos); }
    char read() override { return (char) (_pos < _input.length() ? _input[_pos++] : -1); }
    int read(char* buf, int bytes) override
    {
        int n = 0;
        while (n < bytes && _pos < _input.length())
        {
            buf[n++] = _input[_pos++];
        }
        return n;
    }
    size_t write(const char* buf, int bytes = -1) override
    {
        if (!_connected)
        {
            return 0;  // like Adafruit_USBH_CDC, writes fail when disconnected
        }
        int len = bytes < 0 ? strlen(buf) : bytes;
        written.concat(buf, len);
        return (size_t) len;
    }
    void flush() override {}

    void feed(const String& input) { _input += input; }
    void setConnected(bool connected) { _connected = connected; }
    void clearWritten() { written = ""; }

    String written;

private:
    String _input;
    unsigned int _pos = 0;
    bool _connected = true;
};
