// JobPipeline (src/JobPipeline.hpp): the job pump extracted from main.cpp's
// loop(). A NoReply job must complete immediately, a silent/disconnected
// mount must time out instead of wedging the pipeline, and replies must be
// routed back to their source.
#include "TestHarness.hpp"
#include <unity.h>

#include "FakeSerial.hpp"
#include "Controller.hpp"
#include "InputDevice.hpp"
#include "JobPipeline.hpp"
#include "MessageJob.hpp"

// -- test doubles -----------------------------------------------------------

class StubInput : public IInputDevice
{
public:
    void timeSlice() override {}
    Capabilities supportedCapabilities() override { return Capabilities::RawInput; }
    bool isKeyPressed(KeyPress& pressed) override { (void) pressed; return false; }
    float rawX() override { return 0; }
    float rawY() override { return 0; }
};

static FakeSerial mount;
static StubInput input;

// Fresh objects per test: JobQueue owns raw pointers, so it must not be
// copy-assigned.
static Controller* controller = nullptr;
static JobQueue* queue = nullptr;
static JobPipeline* pipeline = nullptr;

static void resetPipeline()
{
    delete pipeline;
    delete controller;
    delete queue;

    mount = FakeSerial();
    controller = new Controller(&input);
    queue = new JobQueue();
    pipeline = new JobPipeline(&mount, controller);
    Serial.clearOutput();
}

static void tick()
{
    pipeline->tick(*queue);
}

// -- tests -------------------------------------------------------------------

void test_noreply_job_completes_without_mount_reply()
{
    resetPipeline();
    queue->enqueue(new MessageJob(":Mn#", JobSource::FromClient));

    tick();
    TEST_ASSERT_EQUAL_STRING(":Mn#", mount.written.c_str());

    // The pipeline must be idle again: the next queued job is sent on the
    // very next tick without any bytes from the mount.
    queue->enqueue(new MessageJob(":MS#", JobSource::FromClient));
    tick();
    TEST_ASSERT_EQUAL_STRING(":Mn#:MS#", mount.written.c_str());
}

void test_noreply_job_from_controller_signals_completion()
{
    resetPipeline();
    controller->setReply("SENTINEL");
    queue->enqueue(new MessageJob(":RG#", JobSource::FromController));

    tick();
    // Empty reply tells the controller the job finished (no reply expected):
    // the sentinel must have been consumed.
    TEST_ASSERT_EQUAL_STRING("", controller->getReply().c_str());
}

void test_silent_mount_times_out_and_pipeline_recovers()
{
    resetPipeline();
    queue->enqueue(new MessageJob(":GX#", JobSource::FromClient));

    tick();  // sends, enters AwaitingCommandReply
    TEST_ASSERT_EQUAL_STRING(":GX#", mount.written.c_str());

    fake_millis += REPLY_TIMEOUT_MS + 1;
    tick();  // must drop the job and return to idle

    queue->enqueue(new MessageJob(":GR#", JobSource::FromClient));
    tick();
    TEST_ASSERT_EQUAL_STRING(":GX#:GR#", mount.written.c_str());
}

void test_disconnected_mount_times_out()
{
    resetPipeline();
    mount.setConnected(false);
    queue->enqueue(new MessageJob(":GX#", JobSource::FromClient));

    tick();
    fake_millis += REPLY_TIMEOUT_MS + 1;
    tick();

    mount.setConnected(true);
    queue->enqueue(new MessageJob(":GR#", JobSource::FromClient));
    tick();
    TEST_ASSERT_EQUAL_STRING(":GR#", mount.written.c_str());
}

void test_client_reply_forwarded_and_gx_cached()
{
    resetPipeline();
    queue->enqueue(new MessageJob(":GX#", JobSource::FromClient));

    tick();
    mount.feed("TE#");
    tick();

    TEST_ASSERT_EQUAL_STRING("TE#", Serial.output.c_str());
    TEST_ASSERT_EQUAL_STRING("TE#", controller->getLastGX().c_str());
}

void test_controller_job_reply_routed_to_controller()
{
    resetPipeline();
    queue->enqueue(new MessageJob(":GR#", JobSource::FromController));

    tick();
    mount.feed("12:34:56#");
    tick();

    TEST_ASSERT_EQUAL_STRING("12:34:56#", controller->getReply().c_str());
    TEST_ASSERT_EQUAL_STRING("", Serial.output.c_str());  // not leaked to the client
}

void test_controller_gx_reply_is_cached_for_status_screen()
{
    resetPipeline();
    queue->enqueue(new MessageJob(":GX#", JobSource::FromController));

    tick();
    mount.feed("Tracking,--T---,0,0,1,090008,+900000,,#");
    tick();

    // Cached for the idle status screen (like client GX replies) ...
    TEST_ASSERT_EQUAL_STRING("Tracking,--T---,0,0,1,090008,+900000,,#", controller->getLastGX().c_str());
    // ... while still routed to the controller and not leaked to the client.
    TEST_ASSERT_EQUAL_STRING("Tracking,--T---,0,0,1,090008,+900000,,#", controller->getReply().c_str());
    TEST_ASSERT_EQUAL_STRING("", Serial.output.c_str());
}

void test_stale_mount_bytes_are_discarded_while_idle()
{
    resetPipeline();
    // A late reply from an earlier (timed-out) job is sitting in the buffer.
    mount.feed("OpenAstroMount#");

    queue->enqueue(new MessageJob(":GX#", JobSource::FromClient));
    tick();  // idle: must discard the stale bytes, then send :GX#
    TEST_ASSERT_EQUAL_STRING(":GX#", mount.written.c_str());

    tick();  // nothing new from the mount: stale bytes must not pass as a reply
    TEST_ASSERT_EQUAL_STRING("", Serial.output.c_str());

    mount.feed("Tracking,--T---,0,0,1,090008,+900000,,#");
    tick();
    TEST_ASSERT_EQUAL_STRING("Tracking,--T---,0,0,1,090008,+900000,,#", Serial.output.c_str());
}

void test_late_reply_after_timeout_does_not_poison_next_job()
{
    resetPipeline();
    queue->enqueue(new MessageJob(":GVP#", JobSource::FromClient));

    tick();  // send :GVP#
    fake_millis += REPLY_TIMEOUT_MS + 1;
    tick();  // timeout drops the job

    mount.feed("OpenAstroMount#");  // the late reply arrives while idle
    tick();                         // idle tick: must drain it

    queue->enqueue(new MessageJob(":GX#", JobSource::FromClient));
    tick();  // send :GX# into a clean buffer

    mount.feed("Tracking,--T---,0,0,1,090008,+900000,,#");
    tick();
    TEST_ASSERT_EQUAL_STRING("Tracking,--T---,0,0,1,090008,+900000,,#", Serial.output.c_str());
}

// A command the table says nothing about still gets the mount's answer:
// the job waits for it, and nothing else is sent until it arrives.
void test_unlisted_command_waits_for_and_forwards_its_reply()
{
    resetPipeline();
    queue->enqueue(new MessageJob(":XGHD#", JobSource::FromClient));
    tick();  // sends, then waits: the mount answers this one
    TEST_ASSERT_EQUAL_STRING(":XGHD#", mount.written.c_str());

    queue->enqueue(new MessageJob(":GVN#", JobSource::FromClient));
    tick();
    TEST_ASSERT_EQUAL_STRING(":XGHD#", mount.written.c_str());  // not sent yet

    mount.feed("100#");
    tick();
    TEST_ASSERT_EQUAL_STRING("100#", Serial.output.c_str());

    tick();  // idle: now the next command goes out
    TEST_ASSERT_EQUAL_STRING(":XGHD#:GVN#", mount.written.c_str());
}

// :Q# is a stop: the mount answers nothing, so it must not hold up the
// command queued behind it.
void test_no_reply_command_completes_at_once()
{
    resetPipeline();
    queue->enqueue(new MessageJob(":Q#", JobSource::FromClient));
    tick();
    queue->enqueue(new MessageJob(":GVN#", JobSource::FromClient));
    tick();
    TEST_ASSERT_EQUAL_STRING(":Q#:GVN#", mount.written.c_str());
}

// A reply that started but never finished must not leave its bytes in front
// of the answer to the next command.
void test_partial_reply_then_timeout_does_not_poison_next_reply()
{
    resetPipeline();
    queue->enqueue(new MessageJob(":GX#", JobSource::FromClient));
    tick();                // send
    mount.feed("Tracki");  // the reply starts ...
    tick();
    fake_millis += REPLY_TIMEOUT_MS + 1;
    tick();                // ... but never finishes: the job is dropped

    queue->enqueue(new MessageJob(":GX#", JobSource::FromClient));
    tick();
    mount.feed("Tracking#");
    tick();
    TEST_ASSERT_EQUAL_STRING("Tracking#", Serial.output.c_str());  // not "TrackiTracking#"
}

// A command the mount does not answer at all must not cost the full timeout:
// the mount answers within about 120 ms, so after 300 ms of complete silence
// the job is given up on.
void test_unanswered_command_is_given_up_after_a_short_silence()
{
    resetPipeline();
    queue->enqueue(new MessageJob(":XLGR#", JobSource::FromClient));  // not a byte comes back
    tick();
    queue->enqueue(new MessageJob(":GVN#", JobSource::FromClient));

    fake_millis += 299;
    tick();
    TEST_ASSERT_EQUAL_STRING(":XLGR#", mount.written.c_str());  // still waiting

    fake_millis += 2;
    tick();  // gives up
    tick();  // the next command goes out, long before REPLY_TIMEOUT_MS
    TEST_ASSERT_EQUAL_STRING(":XLGR#:GVN#", mount.written.c_str());
}

// Once the mount has started to answer, the short silence limit must not cut
// the reply off: the full timeout applies.
void test_started_reply_keeps_the_full_timeout()
{
    resetPipeline();
    queue->enqueue(new MessageJob(":GX#", JobSource::FromClient));
    tick();
    mount.feed("Track");  // the reply has started ...
    tick();

    fake_millis += 1000;  // ... well past the short limit
    tick();
    queue->enqueue(new MessageJob(":GVN#", JobSource::FromClient));
    tick();
    TEST_ASSERT_EQUAL_STRING(":GX#", mount.written.c_str());  // still waiting for the rest

    mount.feed("ing#");
    tick();
    TEST_ASSERT_EQUAL_STRING("Tracking#", Serial.output.c_str());
}

// A guide pulse is answered with nothing, and guiding sends them back to
// back: the pulse must not hold up the command queued behind it.
void test_guide_pulse_does_not_hold_up_the_next_command()
{
    resetPipeline();
    queue->enqueue(new MessageJob(":Mgn0200#", JobSource::FromClient));
    tick();
    queue->enqueue(new MessageJob(":GVN#", JobSource::FromClient));
    tick();
    TEST_ASSERT_EQUAL_STRING(":Mgn0200#:GVN#", mount.written.c_str());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_noreply_job_completes_without_mount_reply);
    RUN_TEST(test_noreply_job_from_controller_signals_completion);
    RUN_TEST(test_silent_mount_times_out_and_pipeline_recovers);
    RUN_TEST(test_disconnected_mount_times_out);
    RUN_TEST(test_client_reply_forwarded_and_gx_cached);
    RUN_TEST(test_controller_job_reply_routed_to_controller);
    RUN_TEST(test_controller_gx_reply_is_cached_for_status_screen);
    RUN_TEST(test_stale_mount_bytes_are_discarded_while_idle);
    RUN_TEST(test_late_reply_after_timeout_does_not_poison_next_job);
    RUN_TEST(test_unlisted_command_waits_for_and_forwards_its_reply);
    RUN_TEST(test_no_reply_command_completes_at_once);
    RUN_TEST(test_partial_reply_then_timeout_does_not_poison_next_reply);
    RUN_TEST(test_unanswered_command_is_given_up_after_a_short_silence);
    RUN_TEST(test_started_reply_keeps_the_full_timeout);
    RUN_TEST(test_guide_pulse_does_not_hold_up_the_next_command);
    return UNITY_END();
}
