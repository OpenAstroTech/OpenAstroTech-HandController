// Reply assembly behavior of processSerialFromMount() (src/Serial.cpp).
#include "TestHarness.hpp"
#include <unity.h>

#include "FakeSerial.hpp"
#include "MessageJob.hpp"
#include "Serial.hpp"

void test_double_string_reply_ends_with_hash()
{
    // :SC# replies with two #-terminated strings, e.g. "1" + "Updated..."
    MessageJob job(":SC#", JobSource::FromClient);
    FakeSerial mount;
    mount.feed("1Updated#");
    mount.feed("#");  // terminator of the (empty) second string

    String reply = processSerialFromMount(&job, &mount);
    TEST_ASSERT_EQUAL_STRING("1Updated##", reply.c_str());
}

void test_double_string_reply_across_multiple_calls()
{
    MessageJob job(":SC#", JobSource::FromClient);
    FakeSerial mount;

    mount.feed("1");
    String reply = processSerialFromMount(&job, &mount);
    TEST_ASSERT_EQUAL_STRING("", reply.c_str());  // first string not complete yet

    mount.feed("Updating#");
    reply = processSerialFromMount(&job, &mount);
    TEST_ASSERT_EQUAL_STRING("", reply.c_str());  // second string not complete yet

    mount.feed("#");
    reply = processSerialFromMount(&job, &mount);
    TEST_ASSERT_EQUAL_STRING("1Updating##", reply.c_str());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_double_string_reply_ends_with_hash);
    RUN_TEST(test_double_string_reply_across_multiple_calls);
    return UNITY_END();
}
