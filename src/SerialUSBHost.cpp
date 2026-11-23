#include <Arduino.h>
#include <WString.h>

#include "SerialUSBHost.hpp"

void SerialUsbHost::begin(long baudrate) {
    _host->begin(baudrate);
}

bool SerialUsbHost::connected() {
    // "Connected" here means the mount is present on the host port, not
    // that DTR is active: tuh_cdc_connected() reports the control line the
    // HOST would have to set itself (nobody does), so it is permanently
    // false for this link - which silently disabled every connected()-gated
    // feature (own :GX# polls, the :XGM# hardware query). mounted() is the
    // device-presence state. Do NOT set DTR instead: toward an Arduino
    // bootloader USB-serial chip, DTR pulses the target's reset line.
    return _host->mounted();
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
