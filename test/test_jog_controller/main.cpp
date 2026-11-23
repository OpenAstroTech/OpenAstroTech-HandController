// JogController (src/JogController.hpp): the jog screen's throttle state
// machine. One dial, two axes: a detent moves the active axis one rung of
// the speed ladder through zero into reverse, a short click stops both
// axes while moving or toggles the axis when stopped, and a held press
// (>= 750 ms) stops everything and exits jog mode - consuming the press so
// the release afterward cannot be mistaken for another click.
#include "TestHarness.hpp"
#include <unity.h>

#include "JogController.hpp"
#include <string.h>

// Scriptable input: set the flags, then tick.
class FakeJogInput : public JogInput
{
public:
    bool upPulseFlag = false;
    bool downPulseFlag = false;
    bool selectDownFlag = false;
    bool selectUpFlag = false;
    unsigned long heldFor = 0;

    bool upPulse() override
    {
        bool r = upPulseFlag;
        upPulseFlag = false;
        return r;
    }
    bool downPulse() override
    {
        bool r = downPulseFlag;
        downPulseFlag = false;
        return r;
    }
    bool selectDown() override
    {
        bool r = selectDownFlag;
        selectDownFlag = false;
        return r;
    }
    bool selectUp() override
    {
        bool r = selectUpFlag;
        selectUpFlag = false;
        return r;
    }
    unsigned long selectHeldMs(unsigned long now) override
    {
        (void) now;
        return heldFor;
    }
};

// Drains every queued command into out, in order.
static String drain(JogController& jog, FakeJogInput& in)
{
    String out;
    const char* cmd;
    while ((cmd = jog.tick(&in, 1000)) != nullptr)
    {
        out += cmd;
    }
    return out;
}

// A short click: press, release ~50 ms later.
static void click(FakeJogInput& in)
{
    in.selectDownFlag = true;
    in.heldFor = 50;
    in.selectUpFlag = true;
}

void test_dial_throttles_active_axis_through_ladder()
{
    JogController jog;
    jog.begin();
    jog.setActive(true);
    FakeJogInput in;

    in.upPulseFlag = true;
    TEST_ASSERT_EQUAL_STRING(":XSX0.020#", jog.tick(&in, 1000));
    in.upPulseFlag = true;
    TEST_ASSERT_EQUAL_STRING(":XSX0.050#", jog.tick(&in, 1000));
    in.upPulseFlag = true;
    TEST_ASSERT_EQUAL_STRING(":XSX0.100#", jog.tick(&in, 1000));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.1f, jog.raSpeed());
    TEST_ASSERT_TRUE(jog.isMoving());
    TEST_ASSERT_EQUAL_INT(0, strlen(drain(jog, in).c_str()));
}

void test_down_pulse_crosses_zero_into_reverse()
{
    JogController jog;
    jog.begin();
    jog.setActive(true);
    FakeJogInput in;

    in.upPulseFlag = true;
    drain(jog, in);  // +1 rung
    in.downPulseFlag = true;
    TEST_ASSERT_EQUAL_STRING(":XSX0.000#", jog.tick(&in, 1000));
    in.downPulseFlag = true;
    TEST_ASSERT_EQUAL_STRING(":XSX-0.020#", jog.tick(&in, 1000));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, -0.02f, jog.raSpeed());
}

void test_click_while_moving_stops_both()
{
    JogController jog;
    jog.begin();
    jog.setActive(true);
    FakeJogInput in;

    // RA to rung 2, toggle, DEC to rung 1, toggle back.
    in.upPulseFlag = true;
    drain(jog, in);
    in.upPulseFlag = true;
    drain(jog, in);
    click(in);
    drain(jog, in);  // toggles to DEC, no commands
    in.upPulseFlag = true;
    drain(jog, in);  // :XSY0.020#
    click(in);
    drain(jog, in);  // toggles to RA
    in.upPulseFlag = true;
    drain(jog, in);  // :XSX0.020#
    TEST_ASSERT_TRUE(jog.isMoving());

    click(in);
    String out = drain(jog, in);
    TEST_ASSERT_EQUAL_STRING(":XSX0.000#:XSY0.000#", out.c_str());
    TEST_ASSERT_FALSE(jog.isMoving());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, jog.raSpeed());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, jog.decSpeed());
    TEST_ASSERT_TRUE(jog.isActive());  // a stop is not an exit
}

void test_click_while_stopped_toggles_axis()
{
    JogController jog;
    jog.begin();
    jog.setActive(true);
    FakeJogInput in;

    click(in);
    TEST_ASSERT_EQUAL_INT(0, strlen(drain(jog, in).c_str()));
    TEST_ASSERT_EQUAL_CHAR('D', jog.activeAxis());
    click(in);
    drain(jog, in);
    TEST_ASSERT_EQUAL_CHAR('R', jog.activeAxis());
}

void test_long_press_exits_once_and_consumes_the_press()
{
    JogController jog;
    jog.begin();
    jog.setActive(true);
    FakeJogInput in;

    in.upPulseFlag = true;
    drain(jog, in);  // RA moving

    in.selectDownFlag = true;
    drain(jog, in);  // press edge: no commands yet
    TEST_ASSERT_TRUE(jog.isActive());

    in.heldFor = 800;
    String out = drain(jog, in);  // long press fires
    TEST_ASSERT_EQUAL_STRING(":XSX0.000#:XSY0.000#:XSM0#", out.c_str());
    TEST_ASSERT_FALSE(jog.isActive());
    TEST_ASSERT_FALSE(jog.isMoving());

    // The release afterward must produce nothing: the press was consumed.
    in.selectUpFlag = true;
    in.heldFor = 0;
    TEST_ASSERT_EQUAL_INT(0, strlen(drain(jog, in).c_str()));
    TEST_ASSERT_FALSE(jog.isActive());
}

void test_rungs_clamp_at_ladder_ends()
{
    JogController jog;
    jog.begin();
    jog.setActive(true);
    FakeJogInput in;

    String last;
    for (int i = 0; i < 10; i++)
    {
        in.upPulseFlag = true;
        last = drain(jog, in);
    }
    TEST_ASSERT_EQUAL_STRING(":XSX2.000#", last.c_str());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 2.0f, jog.raSpeed());
}

void test_inactive_tick_yields_nullptr()
{
    JogController jog;
    jog.begin();
    FakeJogInput in;
    in.upPulseFlag = true;
    in.selectDownFlag = true;
    TEST_ASSERT_EQUAL_INT(0, strlen(drain(jog, in).c_str()));
    TEST_ASSERT_FALSE(jog.isActive());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_dial_throttles_active_axis_through_ladder);
    RUN_TEST(test_down_pulse_crosses_zero_into_reverse);
    RUN_TEST(test_click_while_moving_stops_both);
    RUN_TEST(test_click_while_stopped_toggles_axis);
    RUN_TEST(test_long_press_exits_once_and_consumes_the_press);
    RUN_TEST(test_rungs_clamp_at_ladder_ends);
    RUN_TEST(test_inactive_tick_yields_nullptr);
    return UNITY_END();
}
