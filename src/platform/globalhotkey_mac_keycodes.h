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

#endif // GLOBALHOTKEY_MAC_KEYCODES_H
