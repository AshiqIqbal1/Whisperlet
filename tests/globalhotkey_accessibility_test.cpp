// Covers #51: a single-modifier tap key that can't register because
// Accessibility is off must start working once access is granted, without
// a restart. Runs the real globalhotkey_common.cpp against a stand-in
// backend that behaves like the macOS one (the tap only registers while the
// process is trusted), with the trust flag flipped between calls the way a
// grant in System Settings flips AXIsProcessTrusted() mid-session.

#include "globalhotkey.h"

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

bool trusted = false;       // stands in for AXIsProcessTrusted()
bool comboFails = false;    // a key combination taken by another app
bool tapCreateFails = false; // CGEventTapCreate failing although trusted
int registerCalls = 0;
} // namespace

using ModKey = GlobalHotkey::ModKey;

// --- stand-in backend -------------------------------------------------------

struct GlobalHotkey::Impl
{
};

GlobalHotkey::GlobalHotkey(QObject *parent)
    : QObject(parent)
    , m_impl(new Impl)
{
}

GlobalHotkey::~GlobalHotkey()
{
    unregisterHotkey();
    delete m_impl;
}

bool GlobalHotkey::needsAccessibility() const
{
    return m_tapMode && !trusted;
}

bool GlobalHotkey::isSupported(const QKeySequence &)
{
    return true;
}

bool GlobalHotkey::registerNative()
{
    ++registerCalls;
    if (m_tapMode)
        return trusted && !tapCreateFails;
    return !comboFails;
}

void GlobalHotkey::unregisterNative()
{
}

void GlobalHotkey::unregisterHotkey()
{
    unregisterNative();
    m_registered = false;
}

// --- tests ------------------------------------------------------------------

namespace {

void reset()
{
    trusted = false;
    comboFails = false;
    tapCreateFails = false;
    registerCalls = 0;
}

// Launching in tap mode with Accessibility off keeps the saved key, and the
// same object goes live once access is granted.
void testLaunchWithoutAccessibilityThenGrant()
{
    reset();
    GlobalHotkey hotkey;
    check(!hotkey.setModifierTap(ModKey::RightShift), "tap can't register while untrusted");
    check(hotkey.isModifierTapMode(), "tap mode is kept, not rolled back");
    check(hotkey.modifierKey() == ModKey::RightShift, "saved key is kept, not reset to Right Cmd");
    check(hotkey.comboLabel() == GlobalHotkey::modKeyLabel(ModKey::RightShift),
          "footer label names the saved key");
    check(!hotkey.isActive(), "not active yet");
    check(hotkey.needsAccessibility(), "reports Accessibility as the blocker");

    check(!hotkey.retryRegistration(), "retry still fails while untrusted");
    check(!hotkey.isActive(), "still not active while untrusted");

    trusted = true; // user grants access in System Settings
    check(!hotkey.needsAccessibility(), "needsAccessibility() follows the live permission");
    check(hotkey.retryRegistration(), "retry registers once trusted");
    check(hotkey.isActive(), "tap is live after the grant, no restart");
    check(hotkey.modifierKey() == ModKey::RightShift, "the saved key is the one that went live");

    const int calls = registerCalls;
    check(hotkey.retryRegistration(), "retry on a live tap reports success");
    check(registerCalls == calls, "retry on a live tap does not register again");
}

// Choosing tap mode in Settings (hotkey suspended) while untrusted: resume()
// fails for the Accessibility reason, not an "in use" clash, and a later
// retry picks up the grant.
void testChooseInSettingsWithoutAccessibilityThenGrant()
{
    reset();
    GlobalHotkey hotkey;
    check(hotkey.setSequence(QKeySequence(QStringLiteral("Ctrl+Shift+R"))), "combo registers");

    hotkey.suspend();
    check(hotkey.setModifierTap(ModKey::RightAlt), "while suspended the choice is only stored");
    check(hotkey.needsAccessibility(), "Settings can see Accessibility is missing");
    check(!hotkey.retryRegistration(), "retry does nothing while suspended");

    check(!hotkey.resume(), "resume fails while untrusted");
    check(hotkey.needsAccessibility(), "the failure is attributed to Accessibility");
    check(hotkey.isModifierTapMode() && hotkey.modifierKey() == ModKey::RightAlt,
          "the chosen tap key survives the failed resume");

    trusted = true;
    check(hotkey.retryRegistration(), "retry registers after the grant");
    check(hotkey.isActive(), "tap is live after the grant");
}

// Access revoked and granted again in the same run keeps tracking reality.
void testTrustFlipsBothWays()
{
    reset();
    trusted = true;
    GlobalHotkey hotkey;
    check(hotkey.setModifierTap(ModKey::RightCmd), "tap registers while trusted");

    trusted = false;
    check(hotkey.needsAccessibility(), "revocation is seen on the next check");
    hotkey.suspend();
    check(!hotkey.resume(), "re-registering fails once revoked");

    trusted = true;
    check(!hotkey.needsAccessibility(), "the new grant is seen on the next check");
    check(hotkey.retryRegistration(), "and the tap registers again");
}

// A tap that fails for some other reason while trusted still rolls back, so
// a broken tap doesn't leave the user with no shortcut at all.
void testNonAccessibilityFailureStillRollsBack()
{
    reset();
    trusted = true;
    tapCreateFails = true;
    GlobalHotkey hotkey;
    check(hotkey.setSequence(QKeySequence(QStringLiteral("Ctrl+Shift+R"))), "combo registers");
    check(!hotkey.setModifierTap(ModKey::RightAlt), "tap fails although trusted");
    check(!hotkey.isModifierTapMode(), "rolled back to the combo");
    check(hotkey.isActive(), "the combo is registered again");
}

} // namespace

int main()
{
    testLaunchWithoutAccessibilityThenGrant();
    testChooseInSettingsWithoutAccessibilityThenGrant();
    testTrustFlipsBothWays();
    testNonAccessibilityFailureStillRollsBack();

    if (failures) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    std::printf("all checks passed\n");
    return EXIT_SUCCESS;
}
