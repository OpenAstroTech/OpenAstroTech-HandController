#pragma once
#include <Arduino.h>
#include <WString.h>
#include <stdlib.h>
#include <string.h>

// Menu item ids for the on-device menu (see MenuDefinition.hpp for the menu
// definition and docs/superpowers/specs/2026-11-22-display-interaction-
// design.md for the interaction design). Globally unique; must match the
// ids in the menu definition. Each item maps to the LX200 command its
// mount-firmware counterpart performs (handlers under src/core/meade/
// MeadeParser*.cpp in OpenAstroTracker-Firmware are authoritative over its
// protocol docs).
enum MenuCommandId : uint16_t
{
    ID_MENU_ROOT = 1,
    ID_TRACKING = 2,
    ID_JOG = 3,
    ID_SUB_GOTO = 10,
    ID_GOTO_OBJECT = 100,
    ID_GOTO_HOME_Q = 110,
    ID_GOTO_HOME_NO = 111,
    ID_GOTO_HOME_YES = 112,
    ID_GOTO_PARK_Q = 120,
    ID_GOTO_PARK_NO = 121,
    ID_GOTO_PARK_YES = 122,
    ID_GOTO_UNPARK = 103,
    ID_SUB_TARGET = 20,
    ID_TARGET_RA_HOURS = 200,
    ID_TARGET_RA_MINUTES = 201,
    ID_TARGET_RA_SECONDS = 202,
    ID_TARGET_DEC_SIGN = 203,
    ID_TARGET_DEC_DEGREES = 204,
    ID_TARGET_DEC_MINUTES = 205,
    ID_TARGET_DEC_SECONDS = 206,
    ID_TARGET_SLEW = 207,
    ID_SUB_ALIGN = 30,
    ID_ALIGN_AUTOHOME_RA_Q = 310,
    ID_ALIGN_AUTOHOME_RA_NO = 311,
    ID_ALIGN_AUTOHOME_RA_YES = 312,
    ID_ALIGN_AUTOHOME_DEC_Q = 320,
    ID_ALIGN_AUTOHOME_DEC_NO = 321,
    ID_ALIGN_AUTOHOME_DEC_YES = 322,
    ID_ALIGN_SET_HOME = 330,
    ID_ALIGN_AZ_PLUS = 340,
    ID_ALIGN_AZ_MINUS = 341,
    ID_ALIGN_ALT_PLUS = 350,
    ID_ALIGN_ALT_MINUS = 351,
    ID_SUB_SETUP = 40,
    ID_SETUP_HA_HOURS = 402,
    ID_SETUP_HA_MINUTES = 403,
    ID_SETUP_HA_STORE = 404,
    ID_SETUP_DATE_MONTH = 410,
    ID_SETUP_DATE_DAY = 411,
    ID_SETUP_DATE_YEAR = 412,
    ID_SETUP_DATE_STORE = 413,
    ID_SETUP_TIME_HOURS = 420,
    ID_SETUP_TIME_MINUTES = 421,
    ID_SETUP_TIME_SECONDS = 422,
    ID_SETUP_TIME_STORE = 423,
    ID_SETUP_UTC = 430,
    ID_SETUP_UTC_STORE = 431,
    ID_SUB_FOCUS = 50,
    ID_FOCUS_IN = 500,
    ID_FOCUS_OUT = 501,
    ID_FOCUS_STOP = 502,
    ID_FOCUS_RATE = 503,
    ID_STOP_ALL = 90,
    ID_MENU_EXIT = 99,
};

// A place in the sky: the target of a Goto (a point of interest or the
// Target form's state).
struct SkyCoordinate
{
    uint8_t raHours;
    uint8_t raMinutes;
    uint8_t raSeconds;
    bool decNegative;
    uint8_t decDegrees;
    uint8_t decMinutes;
    uint8_t decSeconds;
};

// The Target form's editor state, updated from each item's confirm.
struct TargetState
{
    SkyCoordinate target;
};

// The Setup forms' editor state (HA, date, time, UTC offset), updated from
// each item's confirm.
struct SetupState
{
    uint8_t haHours;
    uint8_t haMinutes;
    uint8_t dateMonth;
    uint8_t dateDay;
    uint8_t dateYear;   // 2-digit year, as :SC takes it
    uint8_t timeHours;
    uint8_t timeMinutes;
    uint8_t timeSeconds;
    int utcOffset;
};

// Maps a menu item id to the single LX200 command the mount should
// receive, or nullptr for containers/editors/composites (composites are
// built with the compose helpers below), the confirm screens' "No"
// actions, and items with no mount side effect. Any LX200 command may be
// returned: MessageJob reads the reply of a command it does not list as
// one '#'-terminated string, and its table holds only the exceptions.
inline const char* menuIdToCommand(uint16_t id)
{
    switch (id)
    {
        // Confirm-screen Yes actions and the direct actions.
        case ID_GOTO_HOME_YES: return ":hF#";
        case ID_GOTO_PARK_YES: return ":hP#";
        // The firmware's Unpark merely restarts tracking; :hU# does exactly
        // that and answers "1".
        case ID_GOTO_UNPARK: return ":hU#";
        // Hall-sensor auto-home. The direction letter is mandatory (a bare
        // :MHR# is rejected by the mount's movement parser); 'r'/'d' are
        // the builder-tunable defaults.
        case ID_ALIGN_AUTOHOME_RA_YES: return ":MHRr#";
        case ID_ALIGN_AUTOHOME_DEC_YES: return ":MHDd#";
        // CTRL menu's set-current-as-home.
        case ID_ALIGN_SET_HOME: return ":SHP#";
        // The firmware's arc-minute AZ/ALT adjustments, one arcminute at a
        // time.
        case ID_ALIGN_AZ_PLUS:   return ":MAZ+1.0#";
        case ID_ALIGN_AZ_MINUS:  return ":MAZ-1.0#";
        case ID_ALIGN_ALT_PLUS:  return ":MAL+1.0#";
        case ID_ALIGN_ALT_MINUS: return ":MAL-1.0#";
        case ID_FOCUS_IN:   return ":F+#";
        case ID_FOCUS_OUT:  return ":F-#";
        case ID_FOCUS_STOP: return ":FQ#";
        // The panic gesture: stops everything INCLUDING tracking (:Qa#
        // would only stop manual moves).
        case ID_STOP_ALL: return ":Q#";
        default: return nullptr;
    }
}

// Maps the Tracking toggle's ON/OFF payload to the tracking commands.
inline const char* trackingToggleToCommand(const char* onOff)
{
    if (onOff == nullptr) return nullptr;
    if (strcmp(onOff, "ON") == 0)  return ":MT1#";
    if (strcmp(onOff, "OFF") == 0) return ":MT0#";
    return nullptr;
}

// Maps the (unused today) Move-rate list to rate commands, kept for the
// sim and any future rate picker.
inline const char* rateOptionToCommand(const char* option)
{
    if (option == nullptr) return nullptr;
    if (strcmp(option, "Guide") == 0)  return ":RG#";
    if (strcmp(option, "Center") == 0) return ":RC#";
    if (strcmp(option, "Find") == 0)   return ":RM#";
    if (strcmp(option, "Max") == 0)    return ":RS#";
    return nullptr;
}

// Maps the Focus "Rate" list item's selected option to a speed preset.
inline const char* focusRateOptionToCommand(const char* option)
{
    if (option == nullptr) return nullptr;
    if (strcmp(option, "1 (smallest)") == 0) return ":F1#";
    if (strcmp(option, "2 (small)") == 0)    return ":F2#";
    if (strcmp(option, "3 (large)") == 0)    return ":F3#";
    if (strcmp(option, "4 (largest)") == 0)  return ":F4#";
    return nullptr;
}

// The mount firmware's points of interest (c722_menuPOI.hpp), in its
// order, behind the menu option texts.
struct PoiEntry
{
    const char* name;
    SkyCoordinate coord;
};

static const PoiEntry POINTS_OF_INTEREST[] = {
    {"Polaris",            {3,  0,  8,  false, 89, 21, 6}},
    {"Altair",             {19, 51, 45, false, 8,  55, 15}},
    {"Arcturus",           {14, 16, 37, false, 19, 5,  21}},
    {"Big Dipper",         {12, 16, 26, false, 56, 55, 7}},
    {"Cederblad 214",      {0,  5,  46, false, 67, 16, 45}},
    {"Deneb (Cygnus)",     {20, 42, 7,  false, 45, 21, 12}},
    {"Elephant Trunk",     {21, 39, 44, false, 57, 35, 31}},
    {"Heart Nebula",       {2,  34, 57, false, 61, 31, 17}},
    {"Lagoon Nebula",      {18, 5,  2,  true,  24, 22, 52}},
    {"M31 Andromeda",      {0,  43, 52, false, 41, 22, 53}},
    {"M33 Triangulum",     {1,  35, 2,  false, 30, 46, 5}},
    {"M42 Orion Nebula",   {5,  36, 18, true,  5,  22, 44}},
    {"M51 Whirlpool",      {13, 30, 45, false, 47, 5,  21}},
    {"M63 Sunflower",      {13, 16, 45, false, 41, 55, 14}},
    {"M81 Bodes Galaxy",   {9,  57, 13, false, 68, 58, 1}},
    {"M101 Pinwheel",      {14, 3,  56, false, 54, 15, 0}},
    {"Navi",               {0,  57, 57, false, 60, 49, 33}},
    {"Pleiades 7 Sisters", {3,  48, 15, false, 24, 10, 54}},
    {"Soul Nebula",        {2,  52, 47, false, 60, 30, 56}},
    {"Vega",               {18, 37, 37, false, 38, 48, 7}},
    {"Veil Nebula",        {20, 51, 28, false, 30, 59, 30}},
};

// Looks up a point of interest by its menu option text.
inline bool poiCoordinates(const char* name, SkyCoordinate* out)
{
    if (name == nullptr || out == nullptr)
    {
        return false;
    }
    for (unsigned i = 0; i < sizeof(POINTS_OF_INTEREST) / sizeof(POINTS_OF_INTEREST[0]); i++)
    {
        if (strcmp(name, POINTS_OF_INTEREST[i].name) == 0)
        {
            *out = POINTS_OF_INTEREST[i].coord;
            return true;
        }
    }
    return false;
}

inline void initTargetState(TargetState* state)
{
    memset(state, 0, sizeof(TargetState));
}

// Applies a Target editor item's confirmed value to the state; returns
// false when the id is not part of the form.
inline bool updateTargetState(TargetState* state, uint16_t id, const char* value)
{
    if (state == nullptr || value == nullptr)
    {
        return false;
    }
    long v = atol(value);
    switch (id)
    {
        case ID_TARGET_RA_HOURS:    state->target.raHours = (uint8_t) v; return true;
        case ID_TARGET_RA_MINUTES:  state->target.raMinutes = (uint8_t) v; return true;
        case ID_TARGET_RA_SECONDS:  state->target.raSeconds = (uint8_t) v; return true;
        case ID_TARGET_DEC_SIGN:    state->target.decNegative = (strcmp(value, "ON") == 0); return true;
        case ID_TARGET_DEC_DEGREES: state->target.decDegrees = (uint8_t) v; return true;
        case ID_TARGET_DEC_MINUTES: state->target.decMinutes = (uint8_t) v; return true;
        case ID_TARGET_DEC_SECONDS: state->target.decSeconds = (uint8_t) v; return true;
        default: return false;
    }
}

inline void targetCoordinate(const TargetState* state, SkyCoordinate* out)
{
    *out = state->target;
}

inline void initSetupState(SetupState* state)
{
    memset(state, 0, sizeof(SetupState));
}

// Applies a Setup editor item's confirmed value to the state; returns
// false when the id is not part of any Setup form. The date year is kept
// as the 2-digit form :SC takes.
inline bool updateSetupState(SetupState* state, uint16_t id, const char* value)
{
    if (state == nullptr || value == nullptr)
    {
        return false;
    }
    long v = atol(value);
    switch (id)
    {
        case ID_SETUP_HA_HOURS:     state->haHours = (uint8_t) v; return true;
        case ID_SETUP_HA_MINUTES:   state->haMinutes = (uint8_t) v; return true;
        case ID_SETUP_DATE_MONTH:   state->dateMonth = (uint8_t) v; return true;
        case ID_SETUP_DATE_DAY:     state->dateDay = (uint8_t) v; return true;
        case ID_SETUP_DATE_YEAR:    state->dateYear = (uint8_t) v; return true;
        case ID_SETUP_TIME_HOURS:   state->timeHours = (uint8_t) v; return true;
        case ID_SETUP_TIME_MINUTES: state->timeMinutes = (uint8_t) v; return true;
        case ID_SETUP_TIME_SECONDS: state->timeSeconds = (uint8_t) v; return true;
        case ID_SETUP_UTC:          state->utcOffset = (int) v; return true;
        default: return false;
    }
}

// Composes the commands the composites enqueue, in exactly the formats the
// mount's Set parser accepts: :SrHH:MM:SS# and :Sd+DD*MM:SS# (sign and
// seconds mandatory), :SHHH:MM# (no sign, bare HH<sep>MM), :SLHH:MM:SS#,
// :SCMM/DD/YY#, :SGsHH# (sign spelled out - the mount rejects an unsigned
// offset).
inline void composeSrCommand(const SkyCoordinate* coord, char* buf, size_t bufSize)
{
    snprintf(buf, bufSize, ":Sr%02u:%02u:%02u#",
             coord->raHours, coord->raMinutes, coord->raSeconds);
}

inline void composeSdCommand(const SkyCoordinate* coord, char* buf, size_t bufSize)
{
    snprintf(buf, bufSize, ":Sd%c%02u*%02u:%02u#",
             coord->decNegative ? '-' : '+',
             coord->decDegrees, coord->decMinutes, coord->decSeconds);
}

inline void composeShCommand(const SetupState* state, char* buf, size_t bufSize)
{
    snprintf(buf, bufSize, ":SH%02u:%02u#", state->haHours, state->haMinutes);
}

inline void composeSlCommand(const SetupState* state, char* buf, size_t bufSize)
{
    snprintf(buf, bufSize, ":SL%02u:%02u:%02u#",
             state->timeHours, state->timeMinutes, state->timeSeconds);
}

inline void composeScCommand(const SetupState* state, char* buf, size_t bufSize)
{
    snprintf(buf, bufSize, ":SC%02u/%02u/%02u#",
             state->dateMonth, state->dateDay, state->dateYear);
}

inline void composeSgCommand(const SetupState* state, char* buf, size_t bufSize)
{
    snprintf(buf, bufSize, ":SG%c%u#",
             state->utcOffset < 0 ? '-' : '+',
             state->utcOffset < 0 ? (unsigned) (-state->utcOffset) : (unsigned) state->utcOffset);
}

// True when firing this id should also drop the user back to the idle
// screen: actions that start a motion or end a session (slew, park,
// unpark, home, autohome, HA store, stop-all). Settings and the confirm
// screens' "No" keep the menu open.
inline bool menuActionClosesMenu(uint16_t id)
{
    switch (id)
    {
        case ID_GOTO_OBJECT:
        case ID_TARGET_SLEW:
        case ID_GOTO_HOME_YES:
        case ID_GOTO_PARK_YES:
        case ID_GOTO_UNPARK:
        case ID_ALIGN_AUTOHOME_RA_YES:
        case ID_ALIGN_AUTOHOME_DEC_YES:
        case ID_SETUP_HA_STORE:
        case ID_STOP_ALL:
            return true;
        default:
            return false;
    }
}

// Splits a menu activity payload of the form "id|value" (the OledMenu
// activity callback format) into its numeric id and the value part (which
// may be empty). Returns false for malformed payloads.
inline bool parseMenuPayload(const char* payload, uint16_t* id, const char** value)
{
    if (payload == nullptr || id == nullptr || value == nullptr)
    {
        return false;
    }
    const char* pipe = strchr(payload, '|');
    if (pipe == nullptr || pipe == payload)
    {
        return false;
    }
    for (const char* p = payload; p < pipe; p++)
    {
        if (*p < '0' || *p > '9')
        {
            return false;
        }
    }
    long parsed = atol(payload);  // stops at '|'
    if (parsed < 0 || parsed > 65535)
    {
        return false;
    }
    *id = (uint16_t) parsed;
    *value = pipe + 1;
    return true;
}
