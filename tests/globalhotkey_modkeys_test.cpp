// Covers the single-modifier tap keys offered in Settings (issue #28 added
// the Left-side variants). Checks each platform lists the right keys with
// distinct picker labels, that existing saved values keep their meaning,
// and that the macOS keycodes tell the left and right keys apart.

#include "globalhotkey.h"
#include "globalhotkey_mac_keycodes.h"

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
}
} // namespace

int main()
{
    testEveryKeyIsListedWithItsOwnLabel();
    testExistingSettingValuesAreUnchanged();
    testMacKeycodesAreSideSpecific();

    if (failures == 0)
        std::printf("All modifier-key tests passed.\n");

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
