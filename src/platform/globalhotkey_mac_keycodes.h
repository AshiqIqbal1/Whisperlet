#ifndef GLOBALHOTKEY_MAC_KEYCODES_H
#define GLOBALHOTKEY_MAC_KEYCODES_H

#include "globalhotkey.h"

#include <cstdint>

// Physical macOS virtual keycode (kVK_*) the modifier-tap event tap watches
// for each ModKey. flagsChanged events carry the side-specific keycode, so
// this is what tells Left ⌘ apart from Right ⌘. Kept free of Carbon so the
// mapping can be unit tested.
inline std::uint16_t macModKeyCode(GlobalHotkey::ModKey key)
{
    switch (key) {
    case GlobalHotkey::ModKey::RightCmd:   return 0x36; // kVK_RightCommand
    case GlobalHotkey::ModKey::RightShift: return 0x3C; // kVK_RightShift
    case GlobalHotkey::ModKey::RightAlt:   return 0x3D; // kVK_RightOption
    case GlobalHotkey::ModKey::RightCtrl:  return 0x3E; // kVK_RightControl
    case GlobalHotkey::ModKey::LeftCmd:    return 0x37; // kVK_Command
    case GlobalHotkey::ModKey::LeftShift:  return 0x38; // kVK_Shift
    case GlobalHotkey::ModKey::LeftAlt:    return 0x3A; // kVK_Option
    case GlobalHotkey::ModKey::LeftCtrl:   return 0x3B; // kVK_Control
    }
    return 0;
}

// Side-specific device flag (NX_DEVICE*KEYMASK) set in a flagsChanged
// event's flags while that physical key is held. Lets the tap tell a press
// from a release even when the other side's key of the same kind is down.
inline std::uint64_t macModKeyDeviceFlag(GlobalHotkey::ModKey key)
{
    switch (key) {
    case GlobalHotkey::ModKey::LeftCtrl:   return 0x00000001; // NX_DEVICELCTLKEYMASK
    case GlobalHotkey::ModKey::LeftShift:  return 0x00000002; // NX_DEVICELSHIFTKEYMASK
    case GlobalHotkey::ModKey::RightShift: return 0x00000004; // NX_DEVICERSHIFTKEYMASK
    case GlobalHotkey::ModKey::LeftCmd:    return 0x00000008; // NX_DEVICELCMDKEYMASK
    case GlobalHotkey::ModKey::RightCmd:   return 0x00000010; // NX_DEVICERCMDKEYMASK
    case GlobalHotkey::ModKey::LeftAlt:    return 0x00000020; // NX_DEVICELALTKEYMASK
    case GlobalHotkey::ModKey::RightAlt:   return 0x00000040; // NX_DEVICERALTKEYMASK
    case GlobalHotkey::ModKey::RightCtrl:  return 0x00002000; // NX_DEVICERCTLKEYMASK
    }
    return 0;
}

// Device-independent flag (kCGEventFlagMask*) for the key's kind, set while
// either side's key of that kind is held.
inline std::uint64_t macModKeyKindFlag(GlobalHotkey::ModKey key)
{
    switch (key) {
    case GlobalHotkey::ModKey::LeftShift:
    case GlobalHotkey::ModKey::RightShift: return 0x00020000; // kCGEventFlagMaskShift
    case GlobalHotkey::ModKey::LeftCtrl:
    case GlobalHotkey::ModKey::RightCtrl:  return 0x00040000; // kCGEventFlagMaskControl
    case GlobalHotkey::ModKey::LeftAlt:
    case GlobalHotkey::ModKey::RightAlt:   return 0x00080000; // kCGEventFlagMaskAlternate
    case GlobalHotkey::ModKey::LeftCmd:
    case GlobalHotkey::ModKey::RightCmd:   return 0x00100000; // kCGEventFlagMaskCommand
    }
    return 0;
}

// Flag bits that show some modifier other than `key` is held: every
// side-specific device flag but its own, plus the kind flags of the other
// three kinds. Caps Lock and Fn are not counted.
inline std::uint64_t macOtherModifierFlags(GlobalHotkey::ModKey key)
{
    constexpr std::uint64_t kAllDevice = 0x0000207F;
    constexpr std::uint64_t kAllKinds = 0x001E0000;
    return (kAllDevice & ~macModKeyDeviceFlag(key)) | (kAllKinds & ~macModKeyKindFlag(key));
}

#endif // GLOBALHOTKEY_MAC_KEYCODES_H
