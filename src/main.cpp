#include <Arduino.h>
#include <WString.h>

#include "configuration.hpp"
#include "MessageJob.hpp"
#include "Controller.hpp"
#include "JobPipeline.hpp"
#include "Joystick.hpp"
#if INFO_DISPLAY == 1
#include "InfoDisplay.hpp"
#endif
#include "Serial.hpp"
#include "SerialUSBHost.hpp"

// USBHost is defined in usbh_helper.h
#include "usbh_helper.h"

#define SERIAL_BAUDRATE 19200

Adafruit_USBH_CDC SerialHost;
tusb_desc_device_t desc_device;

JobQueue jobQueue;

// IInputDevice* device = usbSerialDevice;
ISerial* mountSerial = new SerialUsbHost(&SerialHost);
IInputDevice* device = new Joystick(15, 16);
Controller* controller = new Controller(device);
JobPipeline* pipeline = new JobPipeline(mountSerial, controller);

#if INFO_DISPLAY == 1
InfoDisplayRender* display = new InfoDisplay();
#endif

//------------- Core1 -------------//
void setup1() {
    // configure pio-usb: defined in usbh_helper.h
    rp2040_configure_pio_usb();

    // run host stack on controller (rhport) 1
    // Note: For rp2040 pico-pio-usb, calling USBHost.begin() on core1 will have most of the
    // host bit-banging processing works done in core1 to free up core0 for other works
    USBHost.begin(1);

    // Initialize SerialHost
    mountSerial->begin(19200);
}

void loop1() {
    USBHost.task();
}

//------------- Core0 -------------//

void setup()
{
    delay(1000);
    Serial.begin(SERIAL_BAUDRATE);
    Serial.println("OAT-HC booting.");
    for (int i = 0;i < 5;i++)
    {
        delay(100);
        Serial.print(".");
    }
    Serial.println("OAT-HC started.");
    delay(1000);
#if INFO_DISPLAY == 1

    display->init();
    Serial.println("Display initialized.");
#endif
}

long lastRender = 0;

void loop()
{
    // Check if the connected client (ASCOM/OATControl/NINA) has a job for us
    MessageJob* job = processSerialFromClient();
    if (job != nullptr)
    {
        // digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
        LOG(DEBUG_JOBS, "Received Job: [%s] from client", job->getCommand().c_str());
        // Yes, so put it in the queue
        jobQueue.enqueue(job);
    }

    // Check if the controller has a job for us
    job = controller->timeslice();
    if (job != nullptr)
    {
        // digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
        LOG(DEBUG_JOBS, "Received Job: [%s] from controller", job->getCommand().c_str());
        // Yes, so put it in the queue as well
        jobQueue.enqueue(job);
    }

    // Process one job against the mount (sends it, awaits and routes the reply).
    pipeline->tick(jobQueue);

#if INFO_DISPLAY == 1

    // Run the display
    if (lastRender + 50 < millis())
    {
        display->render();
        lastRender = millis();
    }
#endif
}


//--------------------------------------------------------------------+
// TinyUSB Host callbacks
//--------------------------------------------------------------------+
extern "C" {

    // Invoked when a device with CDC interface is mounted
    // idx is index of cdc interface in the internal pool.
    void tuh_cdc_mount_cb(uint8_t idx) {
        // bind SerialHost object to this interface index
        SerialHost.mount(idx);
        // Serial.println("SerialHost is connected to a new CDC device");
    }

    // Invoked when a device with CDC interface is unmounted
    void tuh_cdc_umount_cb(uint8_t idx) {
        SerialHost.umount(idx);
        // Serial.println("SerialHost is disconnected");
    }
}
