#ifndef GLOBALHOTKEY_TAP_H
#define GLOBALHOTKEY_TAP_H

#include <cstdint>

// Decides when a single-modifier press and release counts as a tap. Shared
// by the macOS event tap and the Windows keyboard hook, and kept free of
// platform headers so the rules can be unit tested on either.
//
// A tap is the target key pressed with no other modifier already held,
// nothing else pressed while it is down, and released within kMaxTapMs.
// A longer hold (about to type a capital, holding Shift while scrolling) is
// ignored, like other tap-to-talk tools.
struct ModifierTapDetector
{
    static constexpr std::uint32_t kMaxTapMs = 400;

    // Times are milliseconds from any monotonic clock. They are 32-bit so
    // the Windows tick count can be used as is: the unsigned subtraction
    // stays correct across its wraparound.
    bool keyDown = false;
    bool pending = false; // armed: key down, nothing else seen yet
    std::uint32_t pressTimeMs = 0;

    // otherModifiersHeld: some other modifier was already down when the
    // target key went down, so this is a chord, not a tap. Repeated presses
    // while the key is held (Windows auto-repeat) keep the first press time.
    void press(std::uint32_t timeMs, bool otherModifiersHeld)
    {
        if (keyDown)
            return;
        keyDown = true;
        pending = !otherModifiersHeld;
        pressTimeMs = timeMs;
    }

    // True when this release completes a tap.
    bool release(std::uint32_t timeMs)
    {
        const bool tap = pending && std::uint32_t(timeMs - pressTimeMs) <= kMaxTapMs;
        keyDown = false;
        pending = false;
        return tap;
    }

    // Another key, modifier or mouse button was used while the target is down.
    void cancel() { pending = false; }

    void reset() { *this = ModifierTapDetector{}; }
};

#endif // GLOBALHOTKEY_TAP_H
