// StatusRefresher (src/StatusRefresher.hpp): the idle status screen asks for
// its own :GX# only when no GX reply (client pass-through or own poll) has
// been seen for STALE_MS, and never more often than once per STALE_MS.
#include "TestHarness.hpp"
#include <unity.h>

#include "Controller.hpp"
#include "InputDevice.hpp"
#include "StatusRefresher.hpp"

class StubInput : public IInputDevice
{
public:
    void timeSlice() override {}
    Capabilities supportedCapabilities() override { return Capabilities::RawInput; }
    bool isKeyPressed(KeyPress& pressed) override { (void) pressed; return false; }
    float rawX() override { return 0; }
    float rawY() override { return 0; }
};

static StubInput input;
static Controller* controller = nullptr;
static StatusRefresher* refresher = nullptr;

static void resetRefresher()
{
    delete refresher;
    delete controller;
    controller = new Controller(&input);
    refresher  = new StatusRefresher(controller);
    fake_millis = 10000;  // well past boot
}

void test_fresh_client_gx_suppresses_own_request()
{
    resetRefresher();
    controller->setLastGX("Tracking#", fake_millis - 500);  // client polled 0.5s ago
    TEST_ASSERT_FALSE(refresher->shouldRequest(fake_millis));
}

void test_stale_gx_triggers_own_request()
{
    resetRefresher();
    controller->setLastGX("Tracking#", fake_millis - 2500);  // nothing for 2.5s
    TEST_ASSERT_TRUE(refresher->shouldRequest(fake_millis));
}

void test_own_requests_are_throttled()
{
    resetRefresher();
    controller->setLastGX("Tracking#", fake_millis - 2500);
    TEST_ASSERT_TRUE(refresher->shouldRequest(fake_millis));  // stale -> request

    // Mount silent (timeout): still stale, but the throttle must hold.
    fake_millis += 500;
    TEST_ASSERT_FALSE(refresher->shouldRequest(fake_millis));

    // Only after a full STALE_MS since our own request may we try again.
    fake_millis += 2000;
    TEST_ASSERT_TRUE(refresher->shouldRequest(fake_millis));
}

void test_own_reply_refreshes_the_cache()
{
    resetRefresher();
    controller->setLastGX("Tracking#", fake_millis - 2500);
    TEST_ASSERT_TRUE(refresher->shouldRequest(fake_millis));

    // Our own poll's reply lands in the cache -> fresh again, no request.
    controller->setLastGX("Tracking#", fake_millis);
    fake_millis += 500;
    TEST_ASSERT_FALSE(refresher->shouldRequest(fake_millis));
}

void test_no_request_before_two_seconds_of_uptime()
{
    resetRefresher();
    fake_millis = 1500;  // boot + 1.5s, nothing ever cached
    TEST_ASSERT_FALSE(refresher->shouldRequest(fake_millis));
    fake_millis = 2500;
    TEST_ASSERT_TRUE(refresher->shouldRequest(fake_millis));
}

void test_active_client_selects_two_second_cadence()
{
    resetRefresher();
    refresher->noteClientActivity(fake_millis);  // client talked just now

    // 1.5s-old GX: fresh enough under the 2s client cadence, so no request
    // (this is the case where a 1s standalone cadence would poll).
    controller->setLastGX("Tracking#", fake_millis - 1500);
    TEST_ASSERT_FALSE(refresher->shouldRequest(fake_millis));

    // Stale past 2s -> request.
    fake_millis += 1000;
    TEST_ASSERT_TRUE(refresher->shouldRequest(fake_millis));
}

void test_silent_client_switches_to_one_second_cadence()
{
    resetRefresher();
    refresher->noteClientActivity(fake_millis - 6000);  // client went quiet 6s ago
    controller->setLastGX("Tracking#", fake_millis - 1500);

    // Standalone mode: 1.5s-old status is already stale -> poll now.
    TEST_ASSERT_TRUE(refresher->shouldRequest(fake_millis));

    // And the next poll follows the 1s cadence (throttle permits at +1.1s
    // while the status stays unrefreshed).
    fake_millis += 1100;
    TEST_ASSERT_TRUE(refresher->shouldRequest(fake_millis));
}

void test_client_quietness_boundary_is_five_seconds()
{
    resetRefresher();
    refresher->noteClientActivity(fake_millis - 4000);  // still "active"
    controller->setLastGX("Tracking#", fake_millis - 1500);
    TEST_ASSERT_FALSE(refresher->shouldRequest(fake_millis));  // 2s cadence holds

    fake_millis += 1100;  // quiet for 5.1s now, status 2.6s old
    TEST_ASSERT_TRUE(refresher->shouldRequest(fake_millis));   // standalone kicked in
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_fresh_client_gx_suppresses_own_request);
    RUN_TEST(test_stale_gx_triggers_own_request);
    RUN_TEST(test_own_requests_are_throttled);
    RUN_TEST(test_own_reply_refreshes_the_cache);
    RUN_TEST(test_no_request_before_two_seconds_of_uptime);
    RUN_TEST(test_active_client_selects_two_second_cadence);
    RUN_TEST(test_silent_client_switches_to_one_second_cadence);
    RUN_TEST(test_client_quietness_boundary_is_five_seconds);
    return UNITY_END();
}
