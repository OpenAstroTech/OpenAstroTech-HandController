// Joystick key detection (src/Joystick.hpp): a centered stick must not
// produce key presses, full deflection must produce each direction, and
// rawX()/rawY() must report their own axis.
#include "TestHarness.hpp"
#include <unity.h>

#include "Joystick.hpp"

static int s_xRaw = 512;
static int s_yRaw = 512;

static int stick_adc(pin_size_t pin)
{
    return pin == 15 ? s_xRaw : s_yRaw;
}

// Run enough time slices to flush the 16-sample moving average.
static void settle(Joystick& stick)
{
    for (int i = 0; i < 20; i++)
    {
        fake_millis += 50;
        stick.timeSlice();
    }
}

void test_centered_stick_generates_no_keys()
{
    Joystick stick(15, 16);
    fake_analogRead = stick_adc;
    s_xRaw = 512;
    s_yRaw = 512;

    for (int i = 0; i < 30; i++)
    {
        fake_millis += 100;  // well past the repeat interval
        stick.timeSlice();
        KeyPress key;
        TEST_ASSERT_FALSE_MESSAGE(stick.isKeyPressed(key), "centered stick produced a key press");
    }
}

static void assertDirection(int xRaw, int yRaw, KeyPress expected)
{
    Joystick stick(15, 16);
    fake_analogRead = stick_adc;
    s_xRaw = xRaw;
    s_yRaw = yRaw;

    settle(stick);
    fake_millis += 100;
    stick.timeSlice();

    KeyPress key = KeyPress::None;
    TEST_ASSERT_TRUE_MESSAGE(stick.isKeyPressed(key), "expected a key press");
    TEST_ASSERT_EQUAL((int) expected, (int) key);
}

void test_full_deflection_generates_all_directions()
{
    assertDirection(0, 512, KeyPress::Left);
    assertDirection(1023, 512, KeyPress::Right);
    assertDirection(512, 0, KeyPress::Up);
    assertDirection(512, 1023, KeyPress::Down);
}

void test_rawX_rawY_report_their_own_axis()
{
    Joystick stick(15, 16);
    fake_analogRead = stick_adc;
    s_xRaw = 0;     // x pinned low  -> rawX() should be negative
    s_yRaw = 1023;  // y pinned high -> rawY() should be positive

    settle(stick);
    TEST_ASSERT_TRUE(stick.rawX() < -0.25f);
    TEST_ASSERT_TRUE(stick.rawY() > 0.25f);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_centered_stick_generates_no_keys);
    RUN_TEST(test_full_deflection_generates_all_directions);
    RUN_TEST(test_rawX_rawY_report_their_own_axis);
    return UNITY_END();
}
