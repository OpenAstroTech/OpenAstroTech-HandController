// USBSerialHost.cpp
#include <Arduino.h>
#include <WString.h>
#include <Adafruit_TinyUSB.h>
#include <tusb.h> // Add this line to include TinyUSB header

#include "USBSerialHost.hpp"
#include "Utility.hpp"

// Global instance for callbacks
USBSerialHost* g_usbSerialHost = nullptr;

// USB device mounted callback
extern "C" void tuh_cdc_mount_cb(uint8_t dev_addr, uint8_t instance, const tusb_desc_interface_t *itf_desc) {
    if (g_usbSerialHost) {
        LOG(DEBUG_INFO, "USB CDC device mounted: address = %d, instance = %d", dev_addr, instance);
        g_usbSerialHost->_dev_addr = dev_addr;
        g_usbSerialHost->_instance = instance;
        g_usbSerialHost->_connected = true;
    }
}

// USB device unmounted callback
extern "C" void tuh_cdc_umount_cb(uint8_t dev_addr, uint8_t instance) {
    if (g_usbSerialHost && g_usbSerialHost->_dev_addr == dev_addr && g_usbSerialHost->_instance == instance) {
        LOG(DEBUG_INFO, "USB CDC device unmounted: address = %d, instance = %d", dev_addr, instance);
        g_usbSerialHost->_connected = false;
    }
}

// USB CDC received data callback
extern "C" void tuh_cdc_rx_cb(uint8_t dev_addr, uint8_t instance) {
    if (g_usbSerialHost && g_usbSerialHost->_dev_addr == dev_addr && g_usbSerialHost->_instance == instance) {
        // Data available to read
        uint8_t buf[64];
        uint32_t count = tuh_cdc_read(dev_addr, instance, buf, sizeof(buf));
        
        if (count > 0) {
            // Convert to string and add to buffer
            for (uint32_t i = 0; i < count; i++) {
                g_usbSerialHost->_serialBuffer += (char)buf[i];
            }
        }
    }
}

// USB CDC line state change callback
extern "C" void tuh_cdc_line_state_cb(uint8_t dev_addr, uint8_t instance, bool dtr, bool rts) {
    if (g_usbSerialHost && g_usbSerialHost->_dev_addr == dev_addr && g_usbSerialHost->_instance == instance) {
        LOG(DEBUG_INFO, "USB CDC line state change: DTR = %d, RTS = %d", dtr, rts);
    }
}

// Implementation
USBSerialHost::USBSerialHost() {
    _hostReady = false;
    _connected = false;
    _dev_addr = 0;
    _instance = 0;
    _currentKey = KeyPress::None;
    
    // Save global instance for callbacks
    g_usbSerialHost = this;
    
    // Initialize USB Host
    usbh.begin();
    
    // Create a task for USB Host
    xTaskCreate(task_function, "USB Host Task", 4096, this, 2, NULL);
}

void USBSerialHost::task_function(void* param) {
    USBSerialHost* self = (USBSerialHost*)param;
    
    while (1) {
        self->usbh.task();
        vTaskDelay(1);
    }
}

void USBSerialHost::timeSlice() {
    // Check if USB Host is ready
    if (!_hostReady && usbh.ready()) {
        LOG(DEBUG_INFO, "USB Host is ready");
        _hostReady = true;
    }
    
    // Process serial data if needed
    // This could include parsing commands and converting to keypresses
    if (_connected && !_serialBuffer.isEmpty()) {
        // Example: Very simple command parsing
        if (_serialBuffer.indexOf("UP") >= 0) {
            _currentKey = KeyPress::Up;
            _serialBuffer.replace("UP", "");
        } else if (_serialBuffer.indexOf("DOWN") >= 0) {
            _currentKey = KeyPress::Down;
            _serialBuffer.replace("DOWN", "");
        } else if (_serialBuffer.indexOf("LEFT") >= 0) {
            _currentKey = KeyPress::Left;
            _serialBuffer.replace("LEFT", "");
        } else if (_serialBuffer.indexOf("RIGHT") >= 0) {
            _currentKey = KeyPress::Right;
            _serialBuffer.replace("RIGHT", "");
        }
        
        // Prevent buffer from growing too large
        if (_serialBuffer.length() > 1024) {
            _serialBuffer = _serialBuffer.substring(_serialBuffer.length() - 512);
        }
    }
}

Capabilities USBSerialHost::supportedCapabilities() {
    return Capabilities::KeyPress;
}

bool USBSerialHost::isKeyPressed(KeyPress& pressed) {
    if (_currentKey != KeyPress::None) {
        pressed = _currentKey;
        _currentKey = KeyPress::None;
        return true;
    }
    return false;
}

float USBSerialHost::rawY() {
    return 0.5f; // Not supported for serial devices
}

float USBSerialHost::rawX() {
    return 0.5f; // Not supported for serial devices
}

void USBSerialHost::write(const uint8_t* buffer, size_t size) {
    if (_connected) {
        tuh_cdc_write(_dev_addr, _instance, buffer, size);
        tuh_cdc_write_flush(_dev_addr, _instance);
    }
}

void USBSerialHost::write(const char* str) {
    if (_connected) {
        tuh_cdc_write(_dev_addr, _instance, (uint8_t*)str, strlen(str));
        tuh_cdc_write_flush(_dev_addr, _instance);
    }
}

int USBSerialHost::available() {
    return _serialBuffer.length();
}

String USBSerialHost::readString() {
    String result = _serialBuffer;
    _serialBuffer = "";
    return result;
}