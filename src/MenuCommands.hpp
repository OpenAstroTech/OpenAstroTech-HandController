#pragma once
#include <Arduino.h>
#include <WString.h>
#include <stdlib.h>
#include <string.h>

// Menu item ids for the on-device menu (see the MENU_DEF in main.cpp and
// the draft in the OledMenu simulator's menu.txt). These are globally
// unique and must match the ids in the menu definition; submenu/container
// ids intentionally have no mount command. The menu mirrors the mount
// firmware's LCD menu (OpenAstroTracker-Firmware, c70-c78), and each item
// maps to the LX200 command its LCD counterpart performs.
enum MenuCommandId : uint16_t
{
    ID_MENU_ROOT = 1,
    ID_SUB_GOTO = 10,
    ID_GOTO_OBJECT = 100,
    ID_GOTO_HOME = 101,
    ID_GOTO_UNPARK = 102,
    ID_GOTO_PARK = 103,
    ID_SUB_TARGET = 20,
    ID_TARGET_RA_HOURS = 200,
    ID_TARGET_RA_MINUTES = 201,
    ID_TARGET_RA_SECONDS = 202,
    ID_TARGET_DEC_SIGN = 203,
    ID_TARGET_DEC_DEGREES = 204,
    ID_TARGET_DEC_MINUTES = 205,
    ID_TARGET_DEC_SECONDS = 206,
    ID_TARGET_SLEW = 207,
    ID_SUB_MOVE = 30,
    ID_MOVE_NORTH = 300,
    ID_MOVE_SOUTH = 301,
    ID_MOVE_EAST = 302,
    ID_MOVE_WEST = 303,
    ID_MOVE_STOP = 304,
    ID_MOVE_RATE = 305,
    ID_MOVE_SET_HOME = 306,
    ID_SUB_CAL = 40,
    ID_CAL_TRACK_SPEED = 402,
    ID_CAL_RA_STEPS = 403,
    ID_CAL_DEC_STEPS = 404,
    ID_CAL_BACKLASH = 405,
    ID_CAL_SET_LOWER = 406,
    ID_CAL_CLEAR_LOWER = 407,
    ID_CAL_SET_UPPER = 408,
    ID_CAL_CLEAR_UPPER = 409,
    ID_CAL_UTC_OFFSET = 410,
    ID_CAL_AZ_PLUS = 411,
    ID_CAL_AZ_MINUS = 412,
    ID_CAL_ALT_PLUS = 413,
    ID_CAL_ALT_MINUS = 414,
    ID_SUB_FOCUS = 50,
    ID_FOCUS_IN = 500,
    ID_FOCUS_OUT = 501,
    ID_FOCUS_STOP = 502,
    ID_FOCUS_RATE = 503,
    ID_SUB_HA = 60,
    ID_HA_HOURS = 600,
    ID_HA_MINUTES = 601,
    ID_HA_STORE = 602,
    ID_SUB_INFO = 70,
    ID_INFO_RA = 700,
    ID_INFO_DEC = 701,
    ID_INFO_SITE = 702,
    ID_INFO_DATE = 703,
    ID_INFO_UTC_OFFSET = 704,
    ID_INFO_LOCAL_TIME = 705,
    ID_INFO_FIRMWARE = 706,
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

// The Target and HA forms' editor state, updated from each item's confirm.
struct TargetState
{
    SkyCoordinate target;
    uint8_t haHours;
    uint8_t haMinutes;
};

// Maps a menu item id to the single LX200 command the mount should
// receive, or nullptr for containers/labels/composites (the composites are
// built with the compose helpers below) and items with no mount side
// effect. Any LX200 command may be returned: MessageJob reads the reply of
// a command it does not list as one '#'-terminated string, and its table
// holds only the exceptions (no reply, one character).
inline const char* menuIdToCommand(uint16_t id)
{
    switch (id)
    {
        case ID_MOVE_NORTH: return ":Mn#";
        case ID_MOVE_SOUTH: return ":Ms#";
        case ID_MOVE_EAST:  return ":Me#";
        case ID_MOVE_WEST:  return ":Mw#";
        // Stops the manual moves only. :Q# would stop tracking as well.
        case ID_MOVE_STOP:  return ":Qa#";
        case ID_MOVE_SET_HOME: return ":SHP#";
        case ID_GOTO_HOME:   return ":hF#";
        // The firmware's Unpark merely restarts tracking; :hU# does exactly
        // that and answers "1".
        case ID_GOTO_UNPARK: return ":hU#";
        case ID_GOTO_PARK:   return ":hP#";
        // DEC limits: uppercase sets to the current position, lowercase clears.
        case ID_CAL_SET_LOWER:   return ":XSDLL#";
        case ID_CAL_CLEAR_LOWER: return ":XSDLl#";
        case ID_CAL_SET_UPPER:   return ":XSDLU#";
        case ID_CAL_CLEAR_UPPER: return ":XSDLu#";
        // The firmware's arc-minute AZ/ALT adjustments, one arcminute at a time.
        case ID_CAL_AZ_PLUS:  return ":MAZ+1.0#";
        case ID_CAL_AZ_MINUS: return ":MAZ-1.0#";
        case ID_CAL_ALT_PLUS: return ":MAL+1.0#";
        case ID_CAL_ALT_MINUS: return ":MAL-1.0#";
        case ID_FOCUS_IN:   return ":F+#";
        case ID_FOCUS_OUT:  return ":F-#";
        case ID_FOCUS_STOP: return ":FQ#";
        default: return nullptr;
    }
}

// Maps the Move "Rate" list item's selected option to a rate command.
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

// Fills buf with the LX200 set command for a Cal value item's confirmed
// value, or returns false when the id is not a value item.
inline bool menuValueToCommand(uint16_t id, const char* value, char* buf, size_t bufSize)
{
    if (value == nullptr || buf == nullptr || bufSize == 0)
    {
        return false;
    }
    switch (id)
    {
        case ID_CAL_TRACK_SPEED: snprintf(buf, bufSize, ":XSS%s#", value); return true;
        case ID_CAL_RA_STEPS:    snprintf(buf, bufSize, ":XSR%s#", value); return true;
        case ID_CAL_DEC_STEPS:   snprintf(buf, bufSize, ":XSD%s#", value); return true;
        case ID_CAL_BACKLASH:    snprintf(buf, bufSize, ":XSB%s#", value); return true;
        case ID_CAL_UTC_OFFSET:
            // The mount requires the sign; the integer editor omits '+'.
            snprintf(buf, bufSize, ":SG%s%s#", value[0] == '-' ? "" : "+", value);
            return true;
        default: return false;
    }
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

// Applies a Target/HA editor item's confirmed value to the state; returns
// false when the id is not part of either form.
inline bool updateTargetState(TargetState* state, uint16_t id, const char* value)
{
    if (state == nullptr || value == nullptr)
    {
        return false;
    }
    long v = atol(value);
    switch (id)
    {
        case ID_TARGET_RA_HOURS:   state->target.raHours = (uint8_t) v; return true;
        case ID_TARGET_RA_MINUTES: state->target.raMinutes = (uint8_t) v; return true;
        case ID_TARGET_RA_SECONDS: state->target.raSeconds = (uint8_t) v; return true;
        case ID_TARGET_DEC_SIGN:   state->target.decNegative = (strcmp(value, "ON") == 0); return true;
        case ID_TARGET_DEC_DEGREES:  state->target.decDegrees = (uint8_t) v; return true;
        case ID_TARGET_DEC_MINUTES:  state->target.decMinutes = (uint8_t) v; return true;
        case ID_TARGET_DEC_SECONDS:  state->target.decSeconds = (uint8_t) v; return true;
        case ID_HA_HOURS:   state->haHours = (uint8_t) v; return true;
        case ID_HA_MINUTES: state->haMinutes = (uint8_t) v; return true;
        default: return false;
    }
}

inline void targetCoordinate(const TargetState* state, SkyCoordinate* out)
{
    *out = state->target;
}

// Composes the commands the composites enqueue: :SrHH:MM:SS#,
// :Sd+DD*MM:SS# (the mount's Set parser requires the sign and seconds),
// and :SHHH:MM#.
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

inline void composeShCommand(const TargetState* state, char* buf, size_t bufSize)
{
    snprintf(buf, bufSize, ":SH%02u:%02u#", state->haHours, state->haMinutes);
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
