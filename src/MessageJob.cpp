#include <Arduino.h>
#include <WString.h>

#include "MessageJob.hpp"
#include "Utility.hpp"
#include "libs/MappedDict/MappedDict.hpp"


long MessageJob::_currentId = 0;

CommandType MessageJob::parseCommandType(String inCmd) const
{
    // Commands whose reply is NOT one '#'-terminated string. Reply types
    // follow the mount firmware's command handlers
    // (OpenAstroTracker-Firmware, src/core/meade/MeadeParser*.cpp), which
    // differ from the protocol documentation in places.
    // Every other command - listed or not, with or without arguments - is
    // read as a '#'-terminated string, which is how the mount answers all of
    // its get-commands.
    // A command matches an entry when it starts with the entry (longest entry
    // first), so an entry also covers commands with arguments: :FP123# -> FP.
    MappedDict<String, CommandType>::DictEntry_t replyExceptions[] = {
        /* The mount sends nothing */
        {F("I"), CommandType::NoReply},
        {F("Q"), CommandType::NoReply},       // :Q# (stops tracking too) and its one-letter variants
        {F("RS"), CommandType::NoReply},      // slew rates
        {F("RM"), CommandType::NoReply},
        {F("RC"), CommandType::NoReply},
        {F("RG"), CommandType::NoReply},
        {F("Mn"), CommandType::NoReply},      // moves
        {F("Me"), CommandType::NoReply},
        {F("Ms"), CommandType::NoReply},
        {F("Mw"), CommandType::NoReply},
        {F("MG"), CommandType::NoReply},      // guide pulses: the handler sends nothing on success
        {F("Mg"), CommandType::NoReply},
        {F("MAZ"), CommandType::NoReply},     // :MAZn.nn#
        {F("MAL"), CommandType::NoReply},     // :MALn.nn#
        {F("hP"), CommandType::NoReply},      // park
        {F("hF"), CommandType::NoReply},      // find home
        {F("XD"), CommandType::NoReply},      // :XDnnn#
        {F("XS"), CommandType::NoReply},      // the :XS...# set-commands
        {F("F+"), CommandType::NoReply},
        {F("F-"), CommandType::NoReply},
        {F("F1"), CommandType::NoReply},      // focuser speed presets
        {F("F2"), CommandType::NoReply},
        {F("F3"), CommandType::NoReply},
        {F("F4"), CommandType::NoReply},
        {F("FM"), CommandType::NoReply},      // :FM<steps>#
        {F("FS"), CommandType::NoReply},
        {F("FF"), CommandType::NoReply},
        {F("FQ"), CommandType::NoReply},
        /* The mount sends one character */
        {F("g"), CommandType::NumberReply},   // :gT..# - every :g... reply is one character
        {F("Sd"), CommandType::NumberReply},
        {F("Sr"), CommandType::NumberReply},
        {F("St"), CommandType::NumberReply},
        {F("Sg"), CommandType::NumberReply},
        {F("SG"), CommandType::NumberReply},
        {F("SL"), CommandType::NumberReply},
        {F("SH"), CommandType::NumberReply},
        {F("SY"), CommandType::NumberReply},
        {F("MS"), CommandType::NumberReply},  // slew to target
        {F("MT"), CommandType::NumberReply},  // :MT1#, :MT0# - tracking on/off
        {F("MX"), CommandType::NumberReply},  // :MXxnnnnn#
        {F("MHR"), CommandType::NumberReply}, // :MHRxn#, :MHDxn# - Hall-sensor homing
        {F("MHD"), CommandType::NumberReply},
        {F("MAAH"), CommandType::NumberReply},
        {F("hU"), CommandType::NumberReply},
        {F("hZ"), CommandType::NumberReply},
        {F("FP"), CommandType::NumberReply},  // :FPnnn#
        {F("FB"), CommandType::NumberReply},
        /* The mount sends two '#'-terminated strings */
        {F("SC"), CommandType::DoubleStringReply},  // :SCMM/DD/YY#
    };

    if (inCmd[0] == ':')
    {
        // Apparently some LX200 implementations put spaces in their commands..... remove them with impunity.
        int spacePos;
        while ((spacePos = inCmd.indexOf(' ')) != -1)
        {
            inCmd.remove(spacePos, 1);
        }

        // Remove '#' if present
        inCmd = inCmd.substring(1);
        if (inCmd.endsWith("#"))
        {
            inCmd.remove(inCmd.length() - 1, 1);
        }

        CommandType rtnType = CommandType::StringReply;
        int longestMatch = -1;
        for (unsigned i = 0; i < ARRAY_SIZE(replyExceptions); i++)
        {
            const int entryLength = (int) replyExceptions[i].in.length();
            if (entryLength > longestMatch && inCmd.startsWith(replyExceptions[i].in))
            {
                longestMatch = entryLength;
                rtnType = replyExceptions[i].out;
            }
        }
        return rtnType;
    }

    return CommandType::NoReply;
}

