// Harness smoke test: proves the native test environment compiles the
// firmware's pure logic with the real ArduinoCore-API String and that the
// existing command parsing behaves as expected. Not tied to any fix.
#include "TestHarness.hpp"
#include <unity.h>

#include "MessageJob.hpp"

void test_string_basics()
{
    String s = String(F(":GX#"));
    s.remove(s.length() - 1, 1);
    TEST_ASSERT_EQUAL_STRING("GX", s.c_str() + 1);
    TEST_ASSERT_TRUE(String("SC").equalsIgnoreCase("sc"));
}

void test_parse_known_commands()
{
    MessageJob gx(":GX#", JobSource::FromClient);
    TEST_ASSERT_EQUAL(CommandType::StringReply, gx.getCommandType());

    MessageJob move(":Mn#", JobSource::FromClient);
    TEST_ASSERT_EQUAL(CommandType::NoReply, move.getCommandType());

    MessageJob setDate(":SC#", JobSource::FromClient);
    TEST_ASSERT_EQUAL(CommandType::DoubleStringReply, setDate.getCommandType());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_string_basics);
    RUN_TEST(test_parse_known_commands);
    return UNITY_END();
}
