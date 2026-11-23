#pragma once
#include <Arduino.h>
#include <WString.h>
#include <string.h>

// Builds the menu definition string (grammar: OledMenu docs/menu-format.md)
// for the trimmed interaction tree, omitting the hardware-dependent blocks
// the mount does not have. The same tree, ungated, lives in the OledMenu
// simulator (sim/menu.txt) with the gating table documented in comments;
// keep the two in sync. Ids are normative in MenuCommands.hpp.
//
// Hardware gating reads the mount's :XGM# reply (cached in the Controller,
// one query per mount connection). Field matching is exact per
// comma-separated field: "FOC" must not match the field "NO_FOC", and
// "AUTO_AZ" must not match "AUTO_AZ_ALT".

// True when `field` appears as one whole comma-separated field of the
// :XGM# reply. A nullptr/empty reply matches nothing (the menu is built
// complete until a reply arrives).
inline bool xgmHasField(const char* xgm, const char* field)
{
    if (xgm == nullptr || field == nullptr || xgm[0] == '\0')
    {
        return false;
    }
    const size_t fieldLen = strlen(field);
    const char* p = xgm;
    while (*p != '\0')
    {
        const char* end = strchr(p, ',');
        if (end == nullptr)
        {
            end = p + strlen(p);
        }
        if ((size_t) (end - p) == fieldLen && strncmp(p, field, fieldLen) == 0)
        {
            return true;
        }
        if (*end == '\0')
        {
            break;
        }
        p = end + 1;
    }
    return false;
}

// The AZ and ALT arc-minute nudges (polar-alignment job). AZ presence is
// reported as AUTO_AZ or AUTO_AZ_ALT, ALT as AUTO_ALT or AUTO_AZ_ALT.
inline bool xgmHasAz(const char* xgm)
{
    return xgmHasField(xgm, "AUTO_AZ") || xgmHasField(xgm, "AUTO_AZ_ALT");
}

inline bool xgmHasAlt(const char* xgm)
{
    return xgmHasField(xgm, "AUTO_ALT") || xgmHasField(xgm, "AUTO_AZ_ALT");
}

// Builds the menu definition. xgm is the cached :XGM# reply (# stripped) or
// nullptr/empty for "hardware unknown": the full menu, since a missing
// profile must not permanently hide features of a mount that has them.
inline String buildMenuDef(const char* xgm)
{
    const bool full = (xgm == nullptr || xgm[0] == '\0');
    const bool hasFoc = full || xgmHasField(xgm, "FOC");
    const bool hasRaHall = full || xgmHasField(xgm, "HSAH");
    const bool hasDecHall = full || xgmHasField(xgm, "HSAV");
    const bool hasAz = full || xgmHasAz(xgm);
    const bool hasAlt = full || xgmHasAlt(xgm);

    String def = F(
        "Menu, 1, \"HandController\"\n"

        "  Toggle, 2, \"Tracking\"\n"
        "  Action, 3, \"Jog\"\n"

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

        // Confirm screens: the cursor lands on the first item, so "No"
        // comes first - a reflexive press (fires on button-down) must be
        // the safe one.
        "    Submenu, 110, \"Home mount?\"\n"
        "      Action, 111, \"No\"\n"
        "      Action, 112, \"Yes\"\n"
        "    Submenu, 120, \"Park mount?\"\n"
        "      Action, 121, \"No\"\n"
        "      Action, 122, \"Yes\"\n"

        "    Action, 103, \"Unpark\"\n"
        "    Submenu, 20, \"Target\"\n"
        "      IntegerRanged, 200, \"RA hours\", 0, 23\n"
        "      IntegerRanged, 201, \"RA minutes\", 0, 59\n"
        "      IntegerRanged, 202, \"RA seconds\", 0, 59\n"
        "      Toggle, 203, \"DEC negative\"\n"
        "      IntegerRanged, 204, \"DEC degrees\", 0, 89\n"
        "      IntegerRanged, 205, \"DEC minutes\", 0, 59\n"
        "      IntegerRanged, 206, \"DEC seconds\", 0, 59\n"
        "      Action, 207, \"Slew\"\n");

    def += F("  Submenu, 30, \"Align\"\n");
    if (hasRaHall)
    {
        def += F(
            "    Submenu, 310, \"Autohome RA?\"\n"
            "      Action, 311, \"No\"\n"
            "      Action, 312, \"Yes\"\n");
    }
    if (hasDecHall)
    {
        def += F(
            "    Submenu, 320, \"Autohome DEC?\"\n"
            "      Action, 321, \"No\"\n"
            "      Action, 322, \"Yes\"\n");
    }
    def += F("    Action, 330, \"Set home here\"\n");
    if (hasAz)
    {
        def += F(
            "    Action, 340, \"AZ +1 arcmin\"\n"
            "    Action, 341, \"AZ -1 arcmin\"\n");
    }
    if (hasAlt)
    {
        def += F(
            "    Action, 350, \"ALT +1 arcmin\"\n"
            "    Action, 351, \"ALT -1 arcmin\"\n");
    }

    def += F(
        "  Submenu, 40, \"Setup\"\n"
        "    IntegerRanged, 402, \"HA hours\", 0, 23\n"
        "    IntegerRanged, 403, \"HA minutes\", 0, 59\n"
        "    Action, 404, \"Store & track\"\n"
        "    IntegerRanged, 410, \"Month\", 1, 12\n"
        "    IntegerRanged, 411, \"Day\", 1, 31\n"
        "    IntegerRanged, 412, \"Year\", 0, 99\n"
        "    Action, 413, \"Store date\"\n"
        "    IntegerRanged, 420, \"Hours\", 0, 23\n"
        "    IntegerRanged, 421, \"Minutes\", 0, 59\n"
        "    IntegerRanged, 422, \"Seconds\", 0, 59\n"
        "    Action, 423, \"Store time\"\n"
        "    Integer, 430, \"UTC offset\"\n"
        "    Action, 431, \"Store UTC\"\n");

    if (hasFoc)
    {
        def += F(
            "  Submenu, 50, \"Focus\"\n"
            "    Action, 500, \"Focus in\"\n"
            "    Action, 501, \"Focus out\"\n"
            "    Action, 502, \"Stop\"\n"
            "    Select, 503, \"Rate\"\n"
            "      Option, \"1 (smallest)\"\n"
            "      Option, \"2 (small)\"\n"
            "      Option, \"3 (large)\"\n"
            "      Option, \"4 (largest)\"\n");
    }

    def += F(
        "  Action, 90, \"Stop all\"\n"
        "  Action, 99, \"Exit\"\n");
    return def;
}
