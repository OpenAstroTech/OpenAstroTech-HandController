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
#include "StatusRefresher.hpp"
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
StatusRefresher statusRefresher(controller);
bool waitForSelectRelease = false;
long lastRender = 0;

// Declarative menu definition (grammar: OledMenu docs/menu-format.md).
// Ids must match the MenuCommandId enum in MenuCommands.hpp. The same
// definition lives in the OledMenu simulator (sim/menu.txt), where it can
// be exercised without hardware; keep the two in sync.
static String buildMenuDef()
{
    String def = F(
        "Menu, 1, \"HandController\"\n"
        "  Submenu, 20, \"Target\"\n"
        "    IntegerRanged, 200, \"RA hours\", 0, 23\n"
        "    IntegerRanged, 201, \"RA minutes\", 0, 59\n"
        "    IntegerRanged, 202, \"RA seconds\", 0, 59\n"
        "    Toggle, 203, \"DEC negative\"\n"
        "    IntegerRanged, 204, \"DEC degrees\", 0, 89\n"
        "    IntegerRanged, 205, \"DEC minutes\", 0, 59\n"
        "    IntegerRanged, 206, \"DEC seconds\", 0, 59\n"
        "    Action, 207, \"Slew\"\n"
        "  Submenu, 10, \"Goto\"\n"
        "    Select, 100, \"Object\"\n"
        "      Option, \"Polaris\"\n"
        "      Option, \"Altair\"\n"
        "      Option, \"Arcturus\"\n"
        "      Option, \"Big Dipper\"\n"
        "      Option, \"Cederblad 214\"\n"
        "      Option, \"Deneb (Cygnus)\"\n"
        "      Option, \"Elephant Trunk\"\n"
        "      Option, \"Heart Nebula\"\n"
        "      Option, \"Lagoon Nebula\"\n"
        "      Option, \"M31 Andromeda\"\n"
        "      Option, \"M33 Triangulum\"\n"
        "      Option, \"M42 Orion Nebula\"\n"
        "      Option, \"M51 Whirlpool\"\n"
        "      Option, \"M63 Sunflower\"\n"
        "      Option, \"M81 Bodes Galaxy\"\n"
        "      Option, \"M101 Pinwheel\"\n"
        "      Option, \"Navi\"\n"
        "      Option, \"Pleiades 7 Sisters\"\n"
        "      Option, \"Soul Nebula\"\n"
        "      Option, \"Vega\"\n"
        "      Option, \"Veil Nebula\"\n"
        "    Action, 101, \"Home\"\n"
        "    Action, 102, \"Unpark\"\n"
        "    Action, 103, \"Park\"\n"
        "  Submenu, 60, \"HA\"\n"
        "    IntegerRanged, 600, \"Hours\", 0, 23\n"
        "    IntegerRanged, 601, \"Minutes\", 0, 59\n"
        "    Action, 602, \"Store & track\"\n"
        "  Submenu, 30, \"Move\"\n"
        "    Action, 300, \"North\"\n"
        "    Action, 301, \"South\"\n"
        "    Action, 302, \"East\"\n"
        "    Action, 303, \"West\"\n"
        "    Action, 304, \"Stop\"\n"
        "    Select, 305, \"Rate\"\n"
        "      Option, \"Guide\"\n"
        "      Option, \"Center\"\n"
        "      Option, \"Find\"\n"
        "      Option, \"Max\"\n"
        "    Action, 306, \"Set home here\"\n"
        "  Submenu, 40, \"Cal\"\n"
        "    Float, 402, \"Track speed\", 3\n"
        "    Float, 403, \"RA steps/deg\", 1\n"
        "    Float, 404, \"DEC steps/deg\", 1\n"
        "    Integer, 405, \"Backlash steps\"\n"
        "    Action, 406, \"Set DEC lower\"\n"
        "    Action, 407, \"Clear DEC lower\"\n"
        "    Action, 408, \"Set DEC upper\"\n"
        "    Action, 409, \"Clear DEC upper\"\n"
        "    Integer, 410, \"UTC offset\"\n"
        "    Action, 411, \"AZ +1 arcmin\"\n"
        "    Action, 412, \"AZ -1 arcmin\"\n"
        "    Action, 413, \"ALT +1 arcmin\"\n"
        "    Action, 414, \"ALT -1 arcmin\"\n"
        "  Submenu, 50, \"Focus\"\n"
        "    Action, 500, \"Focus in\"\n"
        "    Action, 501, \"Focus out\"\n"
        "    Action, 502, \"Stop\"\n"
        "    Select, 503, \"Rate\"\n"
        "      Option, \"1 (smallest)\"\n"
        "      Option, \"2 (small)\"\n"
        "      Option, \"3 (large)\"\n"
        "      Option, \"4 (largest)\"\n"
        "  Submenu, 70, \"Info\"\n"
        "    Label, 700, \"RA\"\n"
        "    Label, 701, \"DEC\"\n"
        "    Label, 702, \"Site\"\n"
        "    Label, 703, \"Date\"\n"
        "    Label, 704, \"UTC offset\"\n"
        "    Label, 705, \"Local time\"\n"
        "    Label, 706, \"Firmware\"\n"
        "  Action, 99, \"Exit\"\n");
    return def;
}

// The Target and HA forms collect their editor values here until Slew or
// Store composes them into commands.
static TargetState menuTargetState;

static void enqueueCommand(const char* command)
{
    LOG(DEBUG_JOBS, "Menu -> %s", command);
    jobQueue.enqueue(new MessageJob(command, JobSource::FromController));
}

// A Goto or a Target slew: set the mount's target coordinates, then slew.
// The queue runs one job at a time, so the three commands keep their order.
static void enqueueSlew(const SkyCoordinate* coord)
{
    char buf[20];
    composeSrCommand(coord, buf, sizeof(buf));
    enqueueCommand(buf);
    composeSdCommand(coord, buf, sizeof(buf));
    enqueueCommand(buf);
    enqueueCommand(":MS#");
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

    // The Target and HA editors only update the pending target...
    if (updateTargetState(&menuTargetState, id, value))
    {
        return;
    }

    // ...single commands go straight through...
    const char* command = menuIdToCommand(id);
    if (command == nullptr && id == ID_MOVE_RATE)
    {
        command = rateOptionToCommand(value);
    }
    if (command == nullptr && id == ID_FOCUS_RATE)
    {
        command = focusRateOptionToCommand(value);
    }
    if (command != nullptr)
    {
        enqueueCommand(command);
        return;
    }

    // ...Cal values compose from the confirmed value...
    char buf[20];
    if (menuValueToCommand(id, value, buf, sizeof(buf)))
    {
        enqueueCommand(buf);
        return;
    }

    // ...and the composites build their command sequences.
    if (id == ID_TARGET_SLEW)
    {
        SkyCoordinate coord;
        targetCoordinate(&menuTargetState, &coord);
        enqueueSlew(&coord);
    }
    else if (id == ID_GOTO_OBJECT)
    {
        SkyCoordinate coord;
        if (poiCoordinates(value, &coord))
        {
            enqueueSlew(&coord);
        }
        else
        {
            LOG(DEBUG_JOBS, "Unknown object '%s'", value);
        }
    }
    else if (id == ID_HA_STORE)
    {
        composeShCommand(&menuTargetState, buf, sizeof(buf));
        enqueueCommand(buf);
        enqueueCommand(":MT1#");  // ...and start tracking, like the mount's HA store
    }
}

// Idle screen while the menu is closed: the mount's own InfoDisplay
// (OpenAstroTracker-Firmware, SSD1306_128x64_Display.cpp) as far as the
// :GX# reply reaches - stepper-state boxes, the status line with its
// bouncing pixel, and the current RA and DEC. The bottom strip shows the
// HC's link state where the mount shows memory/LST/uptime, which are
// mount-local values the HC would have to poll separately.
static void renderStatusScreen()
{
    // :GX# fields: state string, six activity letters (R/D/T/Z/A/F, '-'
    // when idle), RA/DEC/TRK stepper positions, compact current RA and
    // DEC, focus position (empty when not fitted).
    String status = controller->getLastGX();
    status.replace("#", "");
    String fields[8];
    int fieldCount = 0;
    int from       = 0;
    while (fieldCount < 8)
    {
        int to = status.indexOf(',', from);
        if (to == -1)
        {
            to = status.length();
        }
        fields[fieldCount++] = status.substring(from, to);
        if (to == (int) status.length())
        {
            break;
        }
        from = to + 1;
    }

    const bool connected = mountSerial->connected();
    const String& activity = fieldCount > 1 ? fields[1] : String("");
    auto axisActive = [&](int pos)
    {
        return activity.length() > (unsigned) pos && activity[pos] != '-';
    };

    lcd->clear();

    // Stepper-state boxes, filled while the axis runs. The mount shows a
    // GDE box instead of FOC; guiding is not part of the :GX# reply, but
    // the focuser is.
    lcd->setCurrentFont(Bitmap5x7);
    lcd->drawStateRect("RA", axisActive(0), 0, 15, 0);
    lcd->drawStateRect("DEC", axisActive(1), 16, 21, 0);
    lcd->drawStateRect("ALT", axisActive(4), 38, 21, 0);
    lcd->drawStateRect("AZ", axisActive(3), 60, 16, 0);
    lcd->drawStateRect("TRK", axisActive(2), 77, 23, 0);
    lcd->drawStateRect("FOC", axisActive(5), 101, 21, 0);

    // Status line with the bouncing pixel.
    String state = (fieldCount > 0 && fields[0].length() > 0) ? fields[0]
                    : (connected ? String("Waiting") : String("No mount"));
    state.toUpperCase();
    lcd->drawString(4, 14, state.c_str());
    const unsigned long phase = millis() % 1500;
    const int bounce = (phase < 750) ? (int) (phase * 3 / 750) : 3 - (int) ((phase - 750) * 3 / 750);
    lcd->getDisplayDevice()->setPixel(0, 15 - bounce);

    // Current coordinates. The :GX# compact forms are unseparated digit
    // runs (RA HHMMSS, DEC sDDMMSS); give them the separators the mount's
    // own display shows, and fall back to the raw field when they are not.
    String raField  = (fieldCount > 5 && fields[5].length() > 0) ? fields[5] : String("--");
    String decField = (fieldCount > 6 && fields[6].length() > 0) ? fields[6] : String("--");
    if (raField.length() == 6)
    {
        raField = raField.substring(0, 2) + ":" + raField.substring(2, 4) + ":" + raField.substring(4, 6);
    }
    if (decField.length() == 7)
    {
        decField = decField.substring(0, 3) + "*" + decField.substring(3, 5) + ":" + decField.substring(5, 7);
    }
    lcd->drawString(8, 24, (String("RA ") + raField).c_str());
    lcd->drawString(0, 36, (String("DEC ") + decField).c_str());

    // Bottom strip (the 3x5 font has no lowercase).
    lcd->setCurrentFont(Bitmap3x5);
    OLEDDisplay* dev = lcd->getDisplayDevice();
    dev->drawString(0, 59, connected ? "MOUNT" : "----");
    dev->drawString(100, 59, "HC " VERSION);

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

    initTargetState(&menuTargetState);

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
#if INFO_DISPLAY == 1
        // Client presence drives the status-poll cadence (StatusRefresher).
        statusRefresher.noteClientActivity(millis());
#endif
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
            // Keep the idle status screen fresh: piggyback on client :GX#
            // pass-throughs and only send our own poll when none has been
            // seen for 2 s (StatusRefresher throttles this further).
            if (mountSerial->connected() && statusRefresher.shouldRequest(millis()))
            {
                jobQueue.enqueue(new MessageJob(":GX#", JobSource::FromController));
            }
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
