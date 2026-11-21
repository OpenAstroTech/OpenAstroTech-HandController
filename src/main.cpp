#include <Arduino.h>
#include <WString.h>

#include "configuration.hpp"
#include "MessageJob.hpp"
#include "Controller.hpp"
#include "JobPipeline.hpp"
#include "InputDevice.hpp"
#if INFO_DISPLAY == 1
#include "SSD1306_128x64_Display.hpp"
#include "MenuCommands.hpp"
#include "Menu/MenuNavigator.hpp"
#include "Menu/MenuParser.hpp"
#include "Menu/RotaryEncoderInput.hpp"
#include "Version.h"
#endif
#include "Serial.hpp"
#include "SerialUSBHost.hpp"

// USBHost is defined in usbh_helper.h
#include "usbh_helper.h"

#define SERIAL_BAUDRATE 19200

Adafruit_USBH_CDC SerialHost;
tusb_desc_device_t desc_device;

JobQueue jobQueue;

ISerial* mountSerial = new SerialUsbHost(&SerialHost);
// On-device input is the menu's rotary encoder; the controller has no input
// device of its own (yet).
IInputDevice* device = new NullInputDevice();
Controller* controller = new Controller(device);
JobPipeline* pipeline = new JobPipeline(mountSerial, controller);

#if INFO_DISPLAY == 1
SDD1306OLED128x64* lcd = nullptr;
RotaryEncoder* rotaryInput = nullptr;
MenuNavigator* menuNav = nullptr;
bool waitForSelectRelease = false;
long lastRender = 0;

// Declarative menu definition (grammar: OledMenu docs/menu-format.md).
// Ids must match the MenuCommandId enum in MenuCommands.hpp.
static String buildMenuDef()
{
    String def = F(
        "Menu, 1, \"HandController\"\n"
        "  Submenu, 10, \"Move\"\n"
        "    Action, 11, \"North\"\n"
        "    Action, 12, \"South\"\n"
        "    Action, 13, \"East\"\n"
        "    Action, 14, \"West\"\n"
        "    Action, 15, \"Stop\"\n"
        "  Submenu, 20, \"Slew rate\"\n"
        "    Select, 21, \"Rate\"\n"
        "      Option, \"Guide\"\n"
        "      Option, \"Center\"\n"
        "      Option, \"Find\"\n"
        "      Option, \"Max\"\n"
        "  Action, 30, \"Park\"\n"
        "  Action, 31, \"Unpark\"\n"
        "  Action, 32, \"Find home\"\n"
        "  Submenu, 40, \"About\"\n"
        "    Label, 41, \"");
    def += VERSION;
    def += F("\"\n"
             "  Action, 99, \"Exit\"\n");
    return def;
}

// Single activity callback from the menu: route "id|value" payloads to mount
// commands through the job pipeline, or handle UI-only items (Exit) here.
static void onMenuActivity(const char* payload)
{
    uint16_t id = 0;
    const char* value = nullptr;
    if (!parseMenuPayload(payload, &id, &value))
    {
        return;
    }

    if (id == ID_MENU_EXIT)
    {
        menuNav->setActive(false);
        waitForSelectRelease = true;
        return;
    }

    const char* command = menuIdToCommand(id);
    if (command == nullptr && id == ID_RATE_SLEW)
    {
        command = rateOptionToCommand(value);
    }
    if (command != nullptr)
    {
        LOG(DEBUG_JOBS, "Menu id %d -> %s", (int) id, command);
        jobQueue.enqueue(new MessageJob(command, JobSource::FromController));
    }
}

// Idle screen while the menu is closed: mount link state plus the last known
// mount status (populated whenever a client polls :GX#).
static void renderStatusScreen()
{
    lcd->clear();
    lcd->drawString(0, 0, "OAT HC " VERSION);
    lcd->drawString(0, 10, mountSerial->connected() ? "Mount: connected" : "Mount: --");

    String status = controller->getLastGX();
    int y    = 20;
    int from = 0;
    for (int line = 0; line < 5 && from < (int) status.length(); line++)
    {
        int to = status.indexOf(',', from);
        if (to == -1)
        {
            to = status.length();
        }
        String field = status.substring(from, to);
        field.replace("#", "");
        lcd->drawString(0, y, field.c_str());
        y += 9;
        from = to + 1;
    }

    lcd->display();
}
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
    // Created here (not as global objects) so the display and encoder pins
    // are configured after the core is initialized.
    lcd = new SDD1306OLED128x64(DISPLAY_SSD1306_I2C_ADDRESS, SDA, SCL);
    lcd->init();
    Serial.println("Display initialized.");

    rotaryInput = new RotaryEncoder(ROTARY_ENCODER_CLK_PIN, ROTARY_ENCODER_DT_PIN, ROTARY_ENCODER_SW_PIN);

    DefaultMenuBuilder menuBuilder;
    if (MenuParser::parse(buildMenuDef().c_str(), &menuBuilder)
        && !menuBuilder.hadError()
        && menuBuilder.getRootMenu() != nullptr)
    {
        menuBuilder.getRootMenu()->setOnMenuActivity(onMenuActivity);
        menuNav = new MenuNavigator(menuBuilder.getRootMenu(), rotaryInput, lcd);
        menuNav->setActive(false);
        Serial.println("Menu initialized.");
    }
    else
    {
        Serial.println("Menu parse failed.");
    }
#endif
}

void loop()
{
    // Check if the connected client (ASCOM/OATControl/NINA) has a job for us
    MessageJob* job = processSerialFromClient();
    if (job != nullptr)
    {
        LOG(DEBUG_JOBS, "Received Job: [%s] from client", job->getCommand().c_str());
        jobQueue.enqueue(job);
    }

    // Check if the controller has a job for us
    job = controller->timeslice();
    if (job != nullptr)
    {
        LOG(DEBUG_JOBS, "Received Job: [%s] from controller", job->getCommand().c_str());
        jobQueue.enqueue(job);
    }

    // Process one job against the mount (sends, awaits and routes the reply).
    pipeline->tick(jobQueue);

#if INFO_DISPLAY == 1
    if (menuNav != nullptr)
    {
        // Poll the encoder every loop: its debounce needs frequent samples
        // and the loop is non-blocking.
        rotaryInput->realTimeUpdate();

        if (!menuNav->isActive())
        {
            // A click opens the menu; wait for the release so the opening
            // click doesn't immediately register as SELECT.
            if (rotaryInput->isSelectPressed() && !waitForSelectRelease)
            {
                menuNav->setActive(true);
            }
            else if (!rotaryInput->isSelectPressed() && waitForSelectRelease)
            {
                waitForSelectRelease = false;
            }
        }

        if (menuNav->update() && menuNav->isActive())
        {
            lcd->clear();
            menuNav->render();
            lcd->display();
            lastRender = millis();
        }
        else if (!menuNav->isActive() && lastRender + 250 < millis())
        {
            renderStatusScreen();
            lastRender = millis();
        }
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
        LOG(DEBUG_CTRL, "SerialHost is connected to a new CDC device.");
    }

    // Invoked when a device with CDC interface is unmounted
    void tuh_cdc_umount_cb(uint8_t idx) {
        SerialHost.umount(idx);
        LOG(DEBUG_CTRL, "SerialHost is disconnected.");
    }

    // Device-level attach/detach logging: shows whether a device enumerates
    // on the host port at all, independent of any class driver binding.
    void tuh_mount_cb(uint8_t daddr) {
        uint16_t vid, pid;
        if (tuh_vid_pid_get(daddr, &vid, &pid)) {
            LOG(DEBUG_CTRL, "USB device mounted: %04x:%04x", vid, pid);
        } else {
            LOG(DEBUG_CTRL, "USB device mounted (no vid/pid).");
        }
    }

    void tuh_umount_cb(uint8_t daddr) {
        LOG(DEBUG_CTRL, "USB device unmounted: addr %d", daddr);
    }
}
