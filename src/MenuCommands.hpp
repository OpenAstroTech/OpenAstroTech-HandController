#pragma once
#include <Arduino.h>
#include <WString.h>
#include <stdlib.h>
#include <string.h>

// Menu item ids for the on-device menu (see the MENU_DEF in main.cpp).
// These are globally unique and must match the ids in the menu definition;
// submenu/container ids intentionally have no mount command.
enum MenuCommandId : uint16_t
{
    ID_MENU_ROOT = 1,
    ID_SUB_MOVE = 10,
    ID_MOVE_NORTH = 11,
    ID_MOVE_SOUTH = 12,
    ID_MOVE_EAST = 13,
    ID_MOVE_WEST = 14,
    ID_MOVE_STOP = 15,
    ID_SUB_RATE = 20,
    ID_RATE_SLEW = 21,
    ID_MOUNT_PARK = 30,
    ID_MOUNT_UNPARK = 31,
    ID_MOUNT_HOME = 32,
    ID_SUB_ABOUT = 40,
    ID_ABOUT_VERSION = 41,
    ID_MENU_EXIT = 99,
};

// Maps a menu item id to the LX200 command the mount should receive, or
// nullptr when the id is a container/label/exit with no mount side effect.
// Only commands from MessageJob's lookup table may be returned here.
inline const char* menuIdToCommand(uint16_t id)
{
    switch (id)
    {
        case ID_MOVE_NORTH: return ":Mn#";
        case ID_MOVE_SOUTH: return ":Ms#";
        case ID_MOVE_EAST:  return ":Me#";
        case ID_MOVE_WEST:  return ":Mw#";
        case ID_MOVE_STOP:  return ":Q#";
        case ID_MOUNT_PARK:   return ":hP#";
        case ID_MOUNT_UNPARK: return ":hU#";
        case ID_MOUNT_HOME:   return ":hF#";
        default: return nullptr;
    }
}

// Maps the "Slew rate" list item's selected option to a rate command.
inline const char* rateOptionToCommand(const char* option)
{
    if (option == nullptr) return nullptr;
    if (strcmp(option, "Guide") == 0)  return ":RG#";
    if (strcmp(option, "Center") == 0) return ":RC#";
    if (strcmp(option, "Find") == 0)   return ":RM#";
    if (strcmp(option, "Max") == 0)    return ":RS#";
    return nullptr;
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
