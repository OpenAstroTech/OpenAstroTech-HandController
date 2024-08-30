#include <Arduino.h>
#include <WString.h>

#include "MessageJob.hpp"
#include "Utility.hpp"
#include "libs/MappedDict/MappedDict.hpp"


long MessageJob::_currentId = 0;

CommandType MessageJob::parseCommandType(String inCmd) const
{
    MappedDict<String, CommandType>::DictEntry_t commandToReplyTypeLookupTable[] = {
        {F("I"), CommandType::NoReply},
        {F("CM"), CommandType::StringReply},
        {F("D"), CommandType::StringReply},
        {F("gT"), CommandType::NumberReply},
        {F("GVP"), CommandType::StringReply},
        {F("GVN"), CommandType::StringReply},
        {F("Gd"), CommandType::StringReply},
        {F("GD"), CommandType::StringReply},
        {F("Gr"), CommandType::StringReply},
        {F("GR"), CommandType::StringReply},
        {F("Gt"), CommandType::StringReply},
        {F("Gg"), CommandType::StringReply},
        {F("Gc"), CommandType::StringReply},
        {F("GG"), CommandType::StringReply},
        {F("Ga"), CommandType::StringReply},
        {F("GL"), CommandType::StringReply},
        {F("GC"), CommandType::StringReply},
        {F("GM"), CommandType::StringReply},
        {F("GN"), CommandType::StringReply},
        {F("GO"), CommandType::StringReply},
        {F("GP"), CommandType::StringReply},
        {F("GT"), CommandType::StringReply},
        {F("GIS"), CommandType::StringReply},
        {F("GIT"), CommandType::StringReply},
        {F("GIG"), CommandType::StringReply},
        {F("GX"), CommandType::StringReply},
        {F("Sd"), CommandType::NumberReply},
        {F("Sr"), CommandType::NumberReply},
        {F("St"), CommandType::NumberReply},
        {F("Sg"), CommandType::NumberReply},
        {F("SC"), CommandType::DoubleStringReply},
        {F("SG"), CommandType::NumberReply},
        {F("SL"), CommandType::NumberReply},
        {F("SH"), CommandType::NumberReply},
        {F("SY"), CommandType::NumberReply},
        {F("MS"), CommandType::NumberReply},
        {F("MG"), CommandType::NumberReply},
        {F("MT"), CommandType::NumberReply},
        {F("MX"), CommandType::NumberReply},
        {F("MH"), CommandType::NumberReply},
        {F("MAAH"), CommandType::NumberReply},
        {F("hU"), CommandType::NumberReply},
        {F("hZ"), CommandType::NumberReply},
        {F("SL"), CommandType::NumberReply},
        {F("SL"), CommandType::NumberReply},
        {F("SL"), CommandType::NumberReply},
        {F("XFR"), CommandType::StringReply},
        {F("XL"), CommandType::StringReply},
        {F("XG"), CommandType::StringReply},
        {F("Fp"), CommandType::StringReply},
        {F("FP"), CommandType::NumberReply},
        {F("FB"), CommandType::NumberReply},
        {F("RS"), CommandType::NoReply},
        {F("RM"), CommandType::NoReply},
        {F("RC"), CommandType::NoReply},
        {F("RG"), CommandType::NoReply},
        {F("Mn"), CommandType::NoReply},
        {F("Me"), CommandType::NoReply},
        {F("Ms"), CommandType::NoReply},
        {F("Mw"), CommandType::NoReply},
        {F("MAZ"), CommandType::NoReply},
        {F("MAL"), CommandType::NoReply},
        {F("hP"), CommandType::NoReply},
        {F("hF"), CommandType::NoReply},
        {F("Qn"), CommandType::NoReply},
        {F("Qe"), CommandType::NoReply},
        {F("Qw"), CommandType::NoReply},
        {F("Qs"), CommandType::NoReply},
        {F("Qa"), CommandType::NoReply},
        {F("Qq"), CommandType::NoReply},
        {F("XD"), CommandType::NoReply},
        {F("XSB"), CommandType::NoReply},
        {F("XSHR"), CommandType::NoReply},
        {F("XSHD"), CommandType::NoReply},
        {F("XSR"), CommandType::NoReply},
        {F("XSD"), CommandType::NoReply},
        {F("XSS"), CommandType::NoReply},
        {F("XST"), CommandType::NoReply},
        {F("XSM"), CommandType::NoReply},
        {F("XSX"), CommandType::NoReply},
        {F("XSY"), CommandType::NoReply},
        {F("F+"), CommandType::NoReply},
        {F("F-"), CommandType::NoReply},
        {F("F1"), CommandType::NoReply},
        {F("F2"), CommandType::NoReply},
        {F("F3"), CommandType::NoReply},
        {F("F4"), CommandType::NoReply},
        {F("FS"), CommandType::NoReply},
        {F("FF"), CommandType::NoReply},
        {F("FQ"), CommandType::NoReply},
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

        // Get the command type
        auto strLookup = MappedDict<String, CommandType>(commandToReplyTypeLookupTable, ARRAY_SIZE(commandToReplyTypeLookupTable));
        CommandType rtnType;
        if (strLookup.tryGet(inCmd, &rtnType))
        {
            return rtnType;
        }
        else
        {
            // Not in the table, special case handling for :Q#
            if (inCmd == "Q")
            {
                return CommandType::NumberReply;
            }
            else
            {
                // unknown command
                LOG(DEBUG_JOBS, "Unknown command! %s", inCmd.c_str());
            }
        }
    }

    return CommandType::NoReply;
}

