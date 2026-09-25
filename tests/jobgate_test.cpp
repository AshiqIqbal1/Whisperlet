#include "jobgate.h"

#include <cstdio>
#include <cstdlib>

namespace {
int failures = 0;

void check(bool condition, const char *what)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

using Refusal = JobGate::Refusal;

// #57: a second job must never reach the engine while one is in flight,
// including while the first is still decoding.
void concurrentJobsAreRefused()
{
    JobGate gate;
    bool recording = true; // dictation in progress
    check(gate.checkStartFileJob(recording) == Refusal::Recording,
          "drop during a dictation is refused");

    recording = false; // dictation stopped: processing starts
    check(gate.begin(), "stopped dictation takes the gate");
    check(gate.checkStartFileJob(recording) == Refusal::Busy,
          "drop while a dictation is processing/transcribing is refused");
    check(!gate.begin(), "a second job cannot take a held gate");
    check(!gate.finish(), "dictation job ends, no close pending");

    // Two files dropped back to back: the first holds the gate from
    // decode start, not just from the whisper pass.
    check(gate.checkStartFileJob(recording) == Refusal::None, "first drop accepted when idle");
    check(gate.begin(), "first drop takes the gate before decoding");
    check(gate.checkStartFileJob(recording) == Refusal::Busy,
          "second drop during the first one's decode is refused");
    check(gate.checkStartRecording() == Refusal::Busy,
          "recording cannot start during a decode either");
    gate.finish();
    check(gate.checkStartFileJob(recording) == Refusal::None, "drop accepted again once idle");
}

// #56: a stop press is never turned away, because nothing else can take
// the gate while a recording is live.
void stopIsAlwaysHonored()
{
    JobGate gate;
    check(gate.checkStartRecording() == Refusal::None, "recording starts when idle");
    const bool recording = true;
    check(gate.checkStartFileJob(recording) == Refusal::Recording,
          "Retry mid-recording is refused");
    check(!gate.busy(), "refused Retry leaves the gate free");
    check(gate.begin(), "stop press takes the gate for its processing");
    gate.finish();

    // Model preload is a job too: recording waits for it.
    check(gate.begin(), "preload takes the gate");
    check(gate.checkStartRecording() == Refusal::Busy, "recording refused during preload");
    gate.finish();
    check(gate.checkStartRecording() == Refusal::None, "recording allowed after preload");
}

// #60: a close mid-job is deferred until the job ends, and no new work
// starts in between.
void closeWaitsForTheJob()
{
    JobGate idle;
    check(idle.requestClose(), "close with nothing in flight goes ahead at once");
    check(idle.checkStartRecording() == Refusal::Closing, "no recording after close");
    check(!idle.begin(), "a stop press after the close starts no work");

    JobGate gate;
    check(gate.begin(), "stopped recording is processing");
    check(!gate.requestClose(), "close mid-processing is deferred");
    check(gate.busy(), "the in-flight job keeps the gate");
    check(gate.checkStartFileJob(false) == Refusal::Closing, "no drop once closing");
    check(gate.checkStartRecording() == Refusal::Closing, "no recording once closing");
    check(!gate.begin(), "no new job while the deferred one runs");
    check(gate.finish(), "job end says the deferred close can go ahead");
    check(gate.requestClose(), "the repeated close is then accepted");
    check(!gate.begin(), "nothing new starts after the close");
}
} // namespace

int main()
{
    concurrentJobsAreRefused();
    stopIsAlwaysHonored();
    closeWaitsForTheJob();

    if (failures == 0)
        std::printf("All job gate tests passed.\n");

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
