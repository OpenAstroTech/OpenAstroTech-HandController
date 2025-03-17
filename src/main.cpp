#include <Arduino.h>
#include <WString.h>

#include "MessageJob.hpp"
#include "Controller.hpp"
#include "Joystick.hpp"
#include "Serial.hpp"
#include "SerialUSBHost.hpp"
// USBHost is defined in usbh_helper.h
//#include "usbh_helper.h"
#include "USBSerialInputDevice.hpp"

#define SERIAL_BAUDRATE 19200
#define D5 14
#define D6 12

enum AppState
{
    AppIdle,
    AwaitingCommandReply,
};

Adafruit_USBH_CDC SerialHost;
tusb_desc_device_t desc_device;

AppState currentState;
JobQueue jobQueue;
MessageJob* activeJob = nullptr;

USBSerialInputDevice* usbSerialDevice = new USBSerialInputDevice();
IInputDevice* device = usbSerialDevice;
//ISerial* serial2 = new SerialUsbHost(&SerialHost);
//IInputDevice* device = new Joystick(15, 16);
Controller controller(device);

//------------- Core1 -------------//
void setup1() {
    // configure pio-usb: defined in usbh_helper.h
    rp2040_configure_pio_usb();

    // run host stack on controller (rhport) 1
    // Note: For rp2040 pico-pio-usb, calling USBHost.begin() on core1 will have most of the
    // host bit-banging processing works done in core1 to free up core0 for other works
    USBHost.begin(1);

    // Initialize SerialHost
//     SerialHost.begin(115200);
}

void loop1() {
    if (usbSerialDevice->isConnected()) {
        // Use USB serial for mount communication instead of SoftwareSerial
        // You would need to adapt the code to use usbSerialDevice->write() 
        // and usbSerialDevice->readString() in place of serial2
    }
}

//------------- Core0 -------------//
void setup()
{
    currentState = AppState::AppIdle;
    // serial2 = new SoftwareSerial(D5, D6);
    Serial.begin(SERIAL_BAUDRATE);
    serial2->begin(SERIAL_BAUDRATE);
    // pinMode(LED_BUILTIN, OUTPUT);
}

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
    job = controller.timeslice();
    if (job != nullptr)
    {
        // digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
        LOG(DEBUG_JOBS, "Received Job: [%s] from controller", job->getCommand().c_str());
        // Yes, so put it in the queue as well
        jobQueue.enqueue(job);
    }

    // See what we're currently doing.
    switch (currentState)
    {
        case AppState::AppIdle:
            {
                // Currently no job is being processed, so check if one is pending and if so, dequeue, send it and await a reply if needed.
                // ASSERT(activeJob == nullptr);
                if (jobQueue.hasJob())
                {
                    activeJob = jobQueue.dequeue();
                    LOG(DEBUG_JOBS, "[Idle] Job [%s] dequeued and processing", activeJob->getCommand().c_str());
                    serial2->write(activeJob->getCommand().c_str());
                    if (activeJob->getCommandType() != CommandType::NoReply)
                    {
                        LOG(DEBUG_JOBS, "[Idle] Job requires reply.");
                        currentState = AppState::AwaitingCommandReply;
                    }
                    else
                    {
                        LOG(DEBUG_JOBS, "[Idle] Job does NOT require reply.");
                        // Stay in Idle mode to retrieve next job
                        delete activeJob;
                        activeJob = nullptr;
                        // digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
                    }
                }
            }
            break;

        case AppState::AwaitingCommandReply:
            {
                // Command was sent, we are awaiting a reply from the mount.
                //ASSERT(activeJob!==nullptr);
                String reply = processSerialFromMount(activeJob, serial2);
                if (!reply.length() == 0)
                {
                    if (activeJob->getSource() == JobSource::FromClient)
                    {
                        LOG(DEBUG_JOBS, "[AwaitReply] Received reply [%s], sending to client.", reply.c_str());
                        // Send back to client
                        if (activeJob->getCommand() == "GX")
                        {
                            // Let controller know the last state (since it's free here)
                            controller.setLastGX(reply, millis());
                        }
                        Serial.print(reply);
                    }
                    else if (activeJob->getSource() == JobSource::FromController)
                    {
                        LOG(DEBUG_JOBS, "[AwaitReply] Received reply [%s], sending to controller.", reply.c_str());
                        // Send to controller
                        controller.setReply(reply);
                    }
                    currentState = AppState::AppIdle;
                    delete activeJob;
                    activeJob = nullptr;
                    // digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
                }
            }
            break;
    }
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
        Serial.println("SerialHost is connected to a new CDC device");
    }

    // Invoked when a device with CDC interface is unmounted
    void tuh_cdc_umount_cb(uint8_t idx) {
        SerialHost.umount(idx);
        Serial.println("SerialHost is disconnected");
    }
}