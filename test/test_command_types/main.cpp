// MessageJob (src/MessageJob.cpp): deciding what reply a command gets. The
// mount answers every get-command with one '#'-terminated string, so that is
// the default for any command - listed or not, with or without arguments.
// The table lists only the exceptions (no reply, one character, two strings);
// a command matches an entry by its beginning, longest entry first, so an
// entry also covers commands with arguments (:FP123# matches FP).
// Reply types follow the mount firmware's command handlers
// (OpenAstroTracker-Firmware, src/core/meade/MeadeParser*.cpp), which differ
// from the protocol documentation in places (guide pulses, for instance).
#include "TestHarness.hpp"
#include <unity.h>

#include "MessageJob.hpp"

static CommandType replyTypeOf(const char* command)
{
    MessageJob job(String(command), JobSource::FromClient);
    return job.getCommandType();
}

// Each must be answered with one '#'-terminated string, including the one
// that takes arguments. None of them is listed: they rely on the default.
void test_xg_get_commands_expect_a_string_reply()
{
    const char* const commands[] = {
        "XGA", "XGAA", "XGAH", "XGB", "XGC12.5*45.0", "XGD", "XGDLR", "XGDP",
        "XGHD", "XGHR", "XGHS", "XGL", "XGM", "XGMS", "XGN", "XGR", "XGS",
        "XGST", "XGT", "XGZ",
    };

    for (unsigned i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
    {
        TEST_ASSERT_EQUAL_MESSAGE(CommandType::StringReply, replyTypeOf((String(":") + commands[i] + "#").c_str()),
                                  commands[i]);
    }
}

void test_gyro_commands_expect_a_string_reply()
{
    const char* const commands[] = {"XL0", "XL1", "XLGC", "XLGR", "XLGT", "XLSR", "XLSP"};

    for (unsigned i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
    {
        TEST_ASSERT_EQUAL_MESSAGE(CommandType::StringReply, replyTypeOf((String(":") + commands[i] + "#").c_str()),
                                  commands[i]);
    }
}

// A command nobody has heard of yet still gets whatever the mount sends.
void test_unlisted_command_expects_a_string_reply()
{
    TEST_ASSERT_EQUAL(CommandType::StringReply, replyTypeOf(":ZZZ#"));
    TEST_ASSERT_EQUAL(CommandType::StringReply, replyTypeOf(":HRR#"));
    TEST_ASSERT_EQUAL(CommandType::StringReply, replyTypeOf(":D#"));
}

// The mount sends nothing for these. Most take arguments, so they can only
// match by their beginning. Guide pulses (:MGn0100#, :Mgn0100#) are documented
// as answering "1", but the firmware's handler sends nothing on success and
// the mount agrees - and a guiding pulse must never be kept waiting.
void test_no_reply_commands()
{
    const char* const commands[] = {
        "I", "Q", "Qa", "Qn", "Qq", "RS", "RM", "RC", "RG",
        "Mn", "Me", "Ms", "Mw", "Mgn0100", "MGn0100", "MAZ1.5", "MAL1.5", "hP", "hF",
        "XD123", "XSA1.5", "XSB10", "XSHR100", "XSHD100", "XSL2.5", "XSR1.5", "XSD2.5",
        "XSDLL10000", "XSDP1000", "XSS1.5", "XST1234", "XSM1", "XSX1.5", "XSY1.5",
        "F+", "F-", "F1", "F2", "F3", "F4", "FM500", "FS", "FF", "FQ",
    };

    for (unsigned i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
    {
        TEST_ASSERT_EQUAL_MESSAGE(CommandType::NoReply, replyTypeOf((String(":") + commands[i] + "#").c_str()),
                                  commands[i]);
    }
}

// The mount sends a single character ("1" / "0") for these, with arguments.
// :gR# is not a command, but every :g... reply is one character. A bare :MH#
// is not one either: only :MHR..# and :MHD..# answer.
void test_one_character_reply_commands()
{
    const char* const commands[] = {
        "gT", "gT123", "gR", "Sd+45*00", "Sr12:30:00", "St+45*00", "Sg+45*00", "SG2.5", "SL15",
        "SH15", "SHP", "SY1", "MS", "MT1", "MT0", "MXr12345", "MHRR", "MHDU",
        "MAAH", "hU", "hZ", "FP123", "FB",
    };

    for (unsigned i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
    {
        TEST_ASSERT_EQUAL_MESSAGE(CommandType::NumberReply, replyTypeOf((String(":") + commands[i] + "#").c_str()),
                                  commands[i]);
    }
}

// :SC# answers with two '#'-terminated strings; the date is the argument.
void test_set_date_expects_two_strings()
{
    TEST_ASSERT_EQUAL(CommandType::DoubleStringReply, replyTypeOf(":SC#"));
    TEST_ASSERT_EQUAL(CommandType::DoubleStringReply, replyTypeOf(":SC12/31/26#"));
}

// Entries that differ only by case must not catch each other's commands:
// :Fp# (a position string) against :FP123# ("1"), :Ms# (a move) against :MS#
// (a slew), :MAAH# ("1") against :MAZ1.5# (nothing).
void test_matching_is_case_sensitive()
{
    TEST_ASSERT_EQUAL(CommandType::StringReply, replyTypeOf(":Fp#"));
    TEST_ASSERT_EQUAL(CommandType::NumberReply, replyTypeOf(":FP123#"));

    TEST_ASSERT_EQUAL(CommandType::NoReply, replyTypeOf(":Ms#"));
    TEST_ASSERT_EQUAL(CommandType::NumberReply, replyTypeOf(":MS#"));

    TEST_ASSERT_EQUAL(CommandType::NumberReply, replyTypeOf(":MAAH#"));
    TEST_ASSERT_EQUAL(CommandType::NoReply, replyTypeOf(":MAZ1.5#"));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_xg_get_commands_expect_a_string_reply);
    RUN_TEST(test_gyro_commands_expect_a_string_reply);
    RUN_TEST(test_unlisted_command_expects_a_string_reply);
    RUN_TEST(test_no_reply_commands);
    RUN_TEST(test_one_character_reply_commands);
    RUN_TEST(test_set_date_expects_two_strings);
    RUN_TEST(test_matching_is_case_sensitive);
    return UNITY_END();
}
