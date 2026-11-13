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

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_noreply_job_completes_without_mount_reply);
    RUN_TEST(test_noreply_job_from_controller_signals_completion);
    RUN_TEST(test_silent_mount_times_out_and_pipeline_recovers);
    RUN_TEST(test_disconnected_mount_times_out);
    RUN_TEST(test_client_reply_forwarded_and_gx_cached);
    RUN_TEST(test_controller_job_reply_routed_to_controller);
    RUN_TEST(test_stale_mount_bytes_are_discarded_while_idle);
    RUN_TEST(test_late_reply_after_timeout_does_not_poison_next_job);
    return UNITY_END();
}
