#ifndef JOBGATE_H
#define JOBGATE_H

// One WhisperEngine, one job at a time. A job is the whole chain that ends
// in the engine: a stopped recording being processed then transcribed, a
// dropped file being decoded then transcribed, a Retry, or the startup
// model preload. The gate is held from the first step to the last, so a
// second job can never slip in between decode/process and whisper.
//
// Drops and Retry are also refused while recording. Recording can only
// start when idle, so a stop press always lands with the gate free and is
// never turned away.
//
// A close that arrives mid-job is deferred: the job runs to the end (so a
// just-stopped recording still gets saved) and finish() reports that the
// close should now go ahead. Nothing new starts in between.
class JobGate
{
public:
    enum class Refusal { None, Busy, Recording, Closing };

    Refusal checkStartRecording() const
    {
        if (m_closing)
            return Refusal::Closing;
        return m_busy ? Refusal::Busy : Refusal::None;
    }

    // Drop or Retry.
    Refusal checkStartFileJob(bool recording) const
    {
        if (m_closing)
            return Refusal::Closing;
        if (m_busy)
            return Refusal::Busy;
        return recording ? Refusal::Recording : Refusal::None;
    }

    // Returns false (and changes nothing) if a job already holds the gate
    // or a close has started.
    bool begin()
    {
        if (m_busy || m_closing)
            return false;
        m_busy = true;
        return true;
    }

    // The job is over. Returns true when a close was deferred for it and
    // should now go ahead.
    bool finish()
    {
        m_busy = false;
        return m_closing;
    }

    // Returns true when the close can go ahead now, false when a job is in
    // flight and finish() will say when. Either way no new job starts.
    bool requestClose()
    {
        m_closing = true;
        return !m_busy;
    }

    bool busy() const { return m_busy; }
    bool closing() const { return m_closing; }

private:
    bool m_busy = false;
    bool m_closing = false;
};

#endif // JOBGATE_H
