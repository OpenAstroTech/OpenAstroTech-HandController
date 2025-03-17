#include <Arduino.h>
#include <WString.h>

#include "SerialUSBHost.hpp"

void SerialUsbHost::begin(long baudrate) {
    _host->begin(baudrate);
}

bool SerialUsbHost::connected() {
    return _host->connected();
}

int SerialUsbHost::available() {
    return _host->available();
}

char SerialUsbHost::read() {
    return (char)_host->read();
}

int SerialUsbHost::read(char* buf, int bytes) {
    return _host->readBytes(buf, bytes);
}

size_t SerialUsbHost::write(const char* buf, int bytes) {
    if (bytes == -1) {
        bytes = strlen(buf);
    }
    size_t bytesWritten = _host->write(buf, bytes);
    _host->flush();
    return bytesWritten;
}

void SerialUsbHost::flush() {
    _host->flush();
}
