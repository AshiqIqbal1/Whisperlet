// Covers the single-modifier tap keys offered in Settings (issue #28 added
// the Left-side variants). Checks each platform lists the right keys with
// distinct picker labels, that existing saved values keep their meaning,
// and that the macOS keycodes tell the left and right keys apart. Also
// covers what counts as a tap (#59): a long hold or a press while another
// modifier is held is not one.

#include "globalhotkey.h"
#include "globalhotkey_mac_keycodes.h"
#include "globalhotkey_tap.h"

#include <QSet>

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

using ModKey = GlobalHotkey::ModKey;

void testEveryKeyIsListedWithItsOwnLabel()
{
    const QList<ModKey> keys = GlobalHotkey::modKeys();
    for (ModKey key : {ModKey::RightCmd, ModKey::RightAlt, ModKey::RightShift, ModKey::RightCtrl,
                       ModKey::LeftShift, ModKey::LeftCtrl})
        check(keys.contains(key), "picker lists the Right keys plus Left Shift and Left Ctrl");
#ifdef Q_OS_MAC
    check(keys.size() == 8, "macOS picker lists all eight modifier keys");
    check(keys.contains(ModKey::LeftCmd), "macOS picker lists Left Cmd");
    check(keys.contains(ModKey::LeftAlt), "macOS picker lists Left Option");
#else
    check(keys.size() == 6, "Windows picker lists six modifier keys");
    check(!keys.contains(ModKey::LeftCmd), "Windows picker omits Left Win");
    check(!keys.contains(ModKey::LeftAlt), "Windows picker omits Left Alt");
#endif

    QSet<QString> labels;
    for (ModKey key : keys) {
        const QString label = GlobalHotkey::modKeyLabel(key);
        check(!label.isEmpty(), "every key has a label");
        labels.insert(label);
    }
    check(labels.size() == keys.size(), "no two keys share a label");
    check(GlobalHotkey::modKeyLabel(ModKey::LeftShift).startsWith(QStringLiteral("Left ")),
          "Left Shift is labelled as a left-side key");
}

void testExistingSettingValuesAreUnchanged()
{
    // modTapKey is stored as the enum's int, so adding keys must not
    // renumber the ones users may already have saved.
    check(int(ModKey::RightCmd) == 0, "RightCmd keeps value 0");
    check(int(ModKey::RightAlt) == 1, "RightAlt keeps value 1");
    check(int(ModKey::RightShift) == 2, "RightShift keeps value 2");
    check(int(ModKey::RightCtrl) == 3, "RightCtrl keeps value 3");
}

void testMacKeycodesAreSideSpecific()
{
    check(macModKeyCode(ModKey::LeftCmd) == 0x37, "Left Cmd is kVK_Command");
    check(macModKeyCode(ModKey::LeftShift) == 0x38, "Left Shift is kVK_Shift");
    check(macModKeyCode(ModKey::LeftAlt) == 0x3A, "Left Option is kVK_Option");
    check(macModKeyCode(ModKey::LeftCtrl) == 0x3B, "Left Control is kVK_Control");
    check(macModKeyCode(ModKey::RightCmd) == 0x36, "Right Cmd is kVK_RightCommand");

    const QList<ModKey> all = {ModKey::RightCmd, ModKey::RightAlt, ModKey::RightShift,
                               ModKey::RightCtrl, ModKey::LeftCmd, ModKey::LeftAlt,
                               ModKey::LeftShift, ModKey::LeftCtrl};
    QSet<std::uint16_t> codes;
    for (ModKey key : all)
        codes.insert(macModKeyCode(key));
    check(codes.size() == all.size(), "every key has its own keycode");

    QSet<std::uint64_t> flags;
    for (ModKey key : all) {
        const std::uint64_t flag = macModKeyDeviceFlag(key);
        check(flag != 0 && (flag & (flag - 1)) == 0, "every key has a single device flag bit");
        flags.insert(flag);
    }
    check(flags.size() == all.size(), "every key has its own device flag");
    check(macModKeyDeviceFlag(ModKey::LeftShift) == 0x02, "Left Shift is NX_DEVICELSHIFTKEYMASK");
    check(macModKeyDeviceFlag(ModKey::RightCtrl) == 0x2000, "Right Control is NX_DEVICERCTLKEYMASK");
}
void testMacOtherModifierFlags()
{
    constexpr std::uint64_t kShift = 0x00020000, kCmd = 0x00100000, kCapsLock = 0x00010000;
    const std::uint64_t rightCmd = macOtherModifierFlags(ModKey::RightCmd);
    check(rightCmd & macModKeyDeviceFlag(ModKey::LeftShift), "Left Shift held counts for Right Cmd");
    check(rightCmd & macModKeyDeviceFlag(ModKey::LeftCmd), "Left Cmd held counts for Right Cmd");
    check(rightCmd & kShift, "Shift held counts for Right Cmd");
    check(!(rightCmd & macModKeyDeviceFlag(ModKey::RightCmd)), "Right Cmd's own flag is not counted");
    check(!(rightCmd & kCmd), "Right Cmd's own kind flag is not counted");
    check(!(rightCmd & kCapsLock), "Caps Lock is not counted");

    const std::uint64_t rightShift = macOtherModifierFlags(ModKey::RightShift);
    check(rightShift & macModKeyDeviceFlag(ModKey::LeftShift), "Left Shift held counts for Right Shift");
    check(!(rightShift & kShift), "Right Shift's own kind flag is not counted");
}

void testQuickTapFires()
{
    ModifierTapDetector d;
    d.press(1000, false);
    check(d.release(1000 + ModifierTapDetector::kMaxTapMs), "release within the limit is a tap");
    d.press(5000, false);
    check(d.release(5100), "a second quick tap fires too");
}

void testLongHoldDoesNotFire()
{
    ModifierTapDetector d;
    d.press(1000, false);
    check(!d.release(1000 + ModifierTapDetector::kMaxTapMs + 1), "release past the limit is not a tap");
    d.press(10000, false);
    check(!d.release(13000), "a several second hold is not a tap");
}

void testAutoRepeatKeepsFirstPressTime()
{
    // Windows repeats WM_KEYDOWN for a held modifier.
    ModifierTapDetector d;
    d.press(1000, false);
    for (std::uint32_t t = 1030; t < 3000; t += 30)
        d.press(t, false);
    check(!d.release(3000), "auto-repeat does not restart the hold timer");
}

void testChordsDoNotFire()
{
    ModifierTapDetector d;
    d.press(1000, true);
    check(!d.release(1050), "press with another modifier already held is not a tap");
    d.press(2000, true);
    d.press(2030, false); // auto-repeat after the other modifier was let go
    check(!d.release(2060), "auto-repeat does not re-arm a rejected press");
    d.press(3000, false);
    d.cancel();
    check(!d.release(3050), "another key pressed while held cancels the tap");
    d.press(4000, false);
    check(d.release(4050), "a clean tap after a chord still fires");
}

void testReleaseWithoutPressDoesNotFire()
{
    ModifierTapDetector d;
    check(!d.release(1000), "release of a key held before the hook started is not a tap");
}

void testTickCountWraparound()
{
    // Windows' tick count is 32-bit and wraps after about 49.7 days.
    ModifierTapDetector d;
    d.press(0xFFFFFF00u, false);
    check(d.release(0x00000010u), "a quick tap across the wrap still fires");
    d.press(0xFFFFFF00u, false);
    check(!d.release(0x00001000u), "a long hold across the wrap does not fire");
}
} // namespace

int main()
{
    testEveryKeyIsListedWithItsOwnLabel();
    testExistingSettingValuesAreUnchanged();
    testMacKeycodesAreSideSpecific();
    testMacOtherModifierFlags();
    testQuickTapFires();
    testLongHoldDoesNotFire();
    testAutoRepeatKeepsFirstPressTime();
    testChordsDoNotFire();
    testReleaseWithoutPressDoesNotFire();
    testTickCountWraparound();

    if (failures == 0)
        std::printf("All modifier-key tests passed.\n");

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
