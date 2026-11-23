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
#include "MenuDefinition.hpp"
#include "JogController.hpp"
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
Menu* menuRoot = nullptr;
StatusRefresher statusRefresher(controller);
bool waitForSelectRelease = false;
long lastRender = 0;

// The editor forms collect their values on-device until Slew or a Store
// composes them into commands (the mount never receives a blind value).
static TargetState menuTargetState;
static SetupState menuSetupState;

// Jog mode: the dial is a speed throttle (JogController), the encoder is
// adapted to the narrow input surface the state machine needs.
static JogController jog;

class RotaryAsJogInput : public JogInput
{
public:
    void begin(RotaryEncoder* encoder) { _encoder = encoder; }

    // Re-syncs the edge tracking when a jog session starts, so a press that
    // is already down (the menu click that opened jog) does not produce a
    // phantom click, and latched detents from menu navigation are dropped.
    void beginSession()
    {
        _selectWasDown = _encoder != nullptr && _encoder->isSelectPressed();
        if (_encoder != nullptr)
        {
            while (_encoder->isUpPressed()) {}
            while (_encoder->isDownPressed()) {}
        }
    }

    bool upPulse() override { return _encoder != nullptr && _encoder->isUpPressed(); }
    bool downPulse() override { return _encoder != nullptr && _encoder->isDownPressed(); }

    bool selectDown() override
    {
        const bool down = _encoder != nullptr && _encoder->isSelectPressed();
        const bool edge = down && !_selectWasDown;
        _selectWasDown = down;
        return edge;
    }

    bool selectUp() override
    {
        const bool down = _encoder != nullptr && _encoder->isSelectPressed();
        const bool edge = !down && _selectWasDown;
        _selectWasDown = down;
        return edge;
    }

    unsigned long selectHeldMs(unsigned long now) override
    {
        (void) now;
        return _encoder != nullptr ? _encoder->getSelectPressDuration() : 0;
    }

private:
    RotaryEncoder* _encoder = nullptr;
    bool _selectWasDown = false;
};

static RotaryAsJogInput rotaryAsJog;

// Hardware profile flow: the :XGM# query retries every 2 s until the mount
// answers (its USB-serial bridge is not ready to answer in the first
// moments after this controller boots), then the menu rebuilds once from
// the reply (MenuDefinition omits the blocks the mount lacks).
static unsigned long lastXGMAttempt = 0;
static const unsigned long XGM_RETRY_MS = 2000;
static bool menuRebuiltFromXGM = false;

// Idle-screen gestures and screen bookkeeping.
static bool reopenMenuOnRelease = false;  // jog exits back into the menu
static bool jogJustActivated = false;
static char lastJogAxis = ' ';
static unsigned long idlePressStart = 0;
static bool idleStopSent = false;
static long lastGXSyncAt = 0;
static const unsigned long IDLE_STOP_ALL_MS = 750;

static void onMenuActivity(const char* payload);  // forward: used by the builder

// Builds a navigator from the (possibly hardware-gated) menu definition.
// Builders and menus leak by design: the menu is built twice per session at
// most (boot, and the hardware-profile rebuild).
static MenuNavigator* buildMenuNavigator(const char* xgmProfile)
{
    String def = buildMenuDef(xgmProfile);
    DefaultMenuBuilder* builder = new DefaultMenuBuilder();
    if (!MenuParser::parse(def.c_str(), builder)
        || builder->hadError()
        || builder->getRootMenu() == nullptr)
    {
        Serial.println("Menu parse failed.");
        return nullptr;
    }
    builder->getRootMenu()->setOnMenuActivity(onMenuActivity);
    menuRoot = builder->getRootMenu();
    MenuNavigator* nav = new MenuNavigator(menuRoot, rotaryInput, lcd);
    nav->setActive(false);
    return nav;
}

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

// True when the cached :GX# reply says tracking is running (activity letter
// T is not '-').
static bool gxTrackingOn()
{
    String gx = controller->getLastGX();
    int firstComma = gx.indexOf(',');
    if (firstComma < 0)
    {
        return false;
    }
    String activity = gx.substring(firstComma + 1);
    int secondComma = activity.indexOf(',');
    if (secondComma >= 0)
    {
        activity = activity.substring(0, secondComma);
    }
    return activity.length() > 2 && activity[2] != '-';
}

// Single activity callback from the menu: route "id|value" payloads to
// mount commands through the job pipeline, or handle UI-only items (Exit,
// Jog, confirm "No") here.
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

    // Confirm screens' "No": step back to the parent menu.
    if (id == ID_GOTO_HOME_NO || id == ID_GOTO_PARK_NO
        || id == ID_ALIGN_AUTOHOME_RA_NO || id == ID_ALIGN_AUTOHOME_DEC_NO)
    {
        menuNav->returnToParent();
        return;
    }

    if (id == ID_JOG)
    {
        menuNav->setActive(false);
        jog.setActive(true);
        jogJustActivated = true;
        rotaryAsJog.beginSession();
        enqueueCommand(":XSM1#");  // manual slew mode on for the jog session
        return;
    }

    if (id == ID_TRACKING)
    {
        const char* cmd = trackingToggleToCommand(value);
        if (cmd != nullptr)
        {
            enqueueCommand(cmd);
        }
        return;
    }

    // The Target and Setup editors only update the pending values...
    if (updateTargetState(&menuTargetState, id, value))
    {
        return;
    }
    if (updateSetupState(&menuSetupState, id, value))
    {
        return;
    }

    // ...single commands go straight through; actions that start a motion
    // or end a session close the menu so the idle screen takes over...
    const char* command = menuIdToCommand(id);
    if (command == nullptr && id == ID_FOCUS_RATE)
    {
        command = focusRateOptionToCommand(value);
    }
    if (command != nullptr)
    {
        enqueueCommand(command);
        if (menuActionClosesMenu(id))
        {
            menuNav->setActive(false);
            waitForSelectRelease = true;
        }
        return;
    }

    // ...and the composites build their command sequences.
    char buf[20];
    if (id == ID_TARGET_SLEW)
    {
        SkyCoordinate coord;
        targetCoordinate(&menuTargetState, &coord);
        enqueueSlew(&coord);
        menuNav->setActive(false);
        waitForSelectRelease = true;
    }
    else if (id == ID_GOTO_OBJECT)
    {
        SkyCoordinate coord;
        if (poiCoordinates(value, &coord))
        {
            enqueueSlew(&coord);
            menuNav->setActive(false);
            waitForSelectRelease = true;
        }
        else
        {
            LOG(DEBUG_JOBS, "Unknown object '%s'", value);
        }
    }
    else if (id == ID_SETUP_HA_STORE)
    {
        composeShCommand(&menuSetupState, buf, sizeof(buf));
        enqueueCommand(buf);
        enqueueCommand(":MT1#");  // ...and start tracking, like the mount's HA store
        menuNav->setActive(false);
        waitForSelectRelease = true;
    }
    else if (id == ID_SETUP_DATE_STORE)
    {
        composeScCommand(&menuSetupState, buf, sizeof(buf));
        enqueueCommand(buf);
    }
    else if (id == ID_SETUP_TIME_STORE)
    {
        composeSlCommand(&menuSetupState, buf, sizeof(buf));
        enqueueCommand(buf);
    }
    else if (id == ID_SETUP_UTC_STORE)
    {
        composeSgCommand(&menuSetupState, buf, sizeof(buf));
        enqueueCommand(buf);
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

// The jog screen: which axis the dial moves and how fast each axis runs.
static void renderJogScreen()
{
    lcd->clear();
    char line[30];
    snprintf(line, sizeof(line), "JOG  %s", jog.activeAxis() == 'R' ? "RA" : "DEC");
    lcd->drawString(0, 0, line);
    snprintf(line, sizeof(line), "%s RA  %+.2f deg/s", jog.activeAxis() == 'R' ? ">" : " ", jog.raSpeed());
    lcd->drawString(1, 14, line);
    snprintf(line, sizeof(line), "%s DEC %+.2f deg/s", jog.activeAxis() == 'D' ? ">" : " ", jog.decSpeed());
    lcd->drawString(1, 24, line);
    lcd->setCurrentFont(Bitmap3x5);
    lcd->getDisplayDevice()->drawString(0, 59, "CLICK STOP/TURN  HOLD EXIT");
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
    initSetupState(&menuSetupState);

    rotaryInput = new RotaryEncoder(ROTARY_ENCODER_CLK_PIN, ROTARY_ENCODER_DT_PIN, ROTARY_ENCODER_SW_PIN);
    rotaryAsJog.begin(rotaryInput);

    // The menu is built before the hardware profile is known: full tree.
    // When the mount answers :XGM#, the loop rebuilds it gated.
    menuNav = buildMenuNavigator(nullptr);
    if (menuNav != nullptr)
    {
        Serial.println("Menu initialized.");
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

        // Ask the mount what hardware it has, once per connection, and
        // rebuild the menu from the profile when the reply arrives. The
        // request retries every 2 s until answered: right after this
        // controller boots, the mount's USB-serial bridge is re-enumerating
        // and the first :XGM# can vanish into the 300 ms silence give-up
        // before the mount is ready to answer.
        if (mountSerial->connected() && controller->getLastXGM().length() == 0
            && millis() - lastXGMAttempt >= XGM_RETRY_MS)
        {
            lastXGMAttempt = millis();
            enqueueCommand(":XGM#");
        }
        if (!menuRebuiltFromXGM && controller->getLastXGM().length() > 0)
        {
            MenuNavigator* rebuilt = buildMenuNavigator(controller->getLastXGM().c_str());
            if (rebuilt != nullptr)
            {
                menuNav = rebuilt;
                Serial.println("Menu rebuilt from hardware profile.");
            }
            menuRebuiltFromXGM = true;  // never retry the parse every loop
        }

        // Keep the Tracking toggle honest while the menu is open.
        if (menuRoot != nullptr && menuNav->isActive() && controller->getLastGXTime() != lastGXSyncAt)
        {
            lastGXSyncAt = controller->getLastGXTime();
            menuRoot->setItemValueById(ID_TRACKING, gxTrackingOn() ? "ON" : "OFF");
        }

        if (jog.isActive())
        {
            // Drain the jog state machine: each returned command goes to
            // the mount in order (NoReply, never queues long).
            bool jogChanged = jogJustActivated;
            const char* cmd;
            while ((cmd = jog.tick(&rotaryAsJog, millis())) != nullptr)
            {
                enqueueCommand(cmd);
                jogChanged = true;
            }
            jogJustActivated = false;

            if (!jog.isActive())
            {
                // The exit long-press: back into the menu once the button
                // is released, at the position it had.
                waitForSelectRelease = true;
                reopenMenuOnRelease = true;
            }
            else if (jogChanged || jog.activeAxis() != lastJogAxis)
            {
                renderJogScreen();
                lastJogAxis = jog.activeAxis();
                lastRender = millis();
            }
        }
        else if (menuNav->isActive())
        {
            if (menuNav->update())
            {
                lcd->clear();
                menuNav->render();
                lcd->display();
                lastRender = millis();
            }
        }
        else
        {
            // Idle: a short press opens the menu, a held press (750 ms) is
            // the Stop Everything gesture.
            const bool selectPressed = rotaryInput->isSelectPressed();
            if (!selectPressed)
            {
                if (waitForSelectRelease)
                {
                    waitForSelectRelease = false;
                }
                if (reopenMenuOnRelease)
                {
                    reopenMenuOnRelease = false;
                    menuNav->setActive(true);
                }
                else if (idlePressStart != 0 && !idleStopSent)
                {
                    menuNav->setActive(true);
                }
                idlePressStart = 0;
                idleStopSent = false;
            }
            else if (!waitForSelectRelease)
            {
                if (idlePressStart == 0)
                {
                    idlePressStart = millis();
                }
                else if (!idleStopSent && millis() - idlePressStart >= IDLE_STOP_ALL_MS)
                {
                    enqueueCommand(":Q#");
                    idleStopSent = true;
                }
            }

            if (!menuNav->isActive() && lastRender + 250 < millis())
            {
                // Keep the idle status screen fresh: piggyback on client
                // :GX# pass-throughs and only send our own poll when none
                // has been seen for 2 s. While jog is active the menu branch
                // above runs instead, so the refresher pause comes free.
                if (mountSerial->connected() && statusRefresher.shouldRequest(millis()))
                {
                    jobQueue.enqueue(new MessageJob(":GX#", JobSource::FromController));
                }
                renderStatusScreen();
                lastRender = millis();
            }
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
#if INFO_DISPLAY == 1
        // A different mount may have different hardware: drop the profile
        // so the loop re-queries :XGM# and rebuilds the menu.
        controller->setLastXGM("");
        lastXGMAttempt = 0;
        menuRebuiltFromXGM = false;
#endif
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
