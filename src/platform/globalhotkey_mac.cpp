// macOS backend for GlobalHotkey.
//
// Combo mode: Carbon RegisterEventHotKey — deprecated-but-standard, and the
// only systemwide-hotkey API that needs no permission (Rectangle, Alfred and
// friends use it).
//
// Modifier-tap mode: bare modifiers can't be hotkeys, so this uses a
// listen-only CGEventTap on flagsChanged/keyDown. That DOES require the
// Accessibility permission — the same one dictation already asks for.
#include "globalhotkey.h"

#include <Carbon/Carbon.h>

#include <QDebug>

namespace {

constexpr UInt32 kHotKeySignature = 'WspF'; // arbitrary 4-char app namespace
constexpr UInt32 kHotKeyId = 1;

// Qt logical key -> Carbon *physical* keycode (ANSI layout positions).
int carbonKeyCode(Qt::Key key)
{
    switch (key) {
    case Qt::Key_A: return kVK_ANSI_A;
    case Qt::Key_B: return kVK_ANSI_B;
    case Qt::Key_C: return kVK_ANSI_C;
    case Qt::Key_D: return kVK_ANSI_D;
    case Qt::Key_E: return kVK_ANSI_E;
    case Qt::Key_F: return kVK_ANSI_F;
    case Qt::Key_G: return kVK_ANSI_G;
    case Qt::Key_H: return kVK_ANSI_H;
    case Qt::Key_I: return kVK_ANSI_I;
    case Qt::Key_J: return kVK_ANSI_J;
    case Qt::Key_K: return kVK_ANSI_K;
    case Qt::Key_L: return kVK_ANSI_L;
    case Qt::Key_M: return kVK_ANSI_M;
    case Qt::Key_N: return kVK_ANSI_N;
    case Qt::Key_O: return kVK_ANSI_O;
    case Qt::Key_P: return kVK_ANSI_P;
    case Qt::Key_Q: return kVK_ANSI_Q;
    case Qt::Key_R: return kVK_ANSI_R;
    case Qt::Key_S: return kVK_ANSI_S;
    case Qt::Key_T: return kVK_ANSI_T;
    case Qt::Key_U: return kVK_ANSI_U;
    case Qt::Key_V: return kVK_ANSI_V;
    case Qt::Key_W: return kVK_ANSI_W;
    case Qt::Key_X: return kVK_ANSI_X;
    case Qt::Key_Y: return kVK_ANSI_Y;
    case Qt::Key_Z: return kVK_ANSI_Z;
    case Qt::Key_0: return kVK_ANSI_0;
    case Qt::Key_1: return kVK_ANSI_1;
    case Qt::Key_2: return kVK_ANSI_2;
    case Qt::Key_3: return kVK_ANSI_3;
    case Qt::Key_4: return kVK_ANSI_4;
    case Qt::Key_5: return kVK_ANSI_5;
    case Qt::Key_6: return kVK_ANSI_6;
    case Qt::Key_7: return kVK_ANSI_7;
    case Qt::Key_8: return kVK_ANSI_8;
    case Qt::Key_9: return kVK_ANSI_9;
    case Qt::Key_F1: return kVK_F1;
    case Qt::Key_F2: return kVK_F2;
    case Qt::Key_F3: return kVK_F3;
    case Qt::Key_F4: return kVK_F4;
    case Qt::Key_F5: return kVK_F5;
    case Qt::Key_F6: return kVK_F6;
    case Qt::Key_F7: return kVK_F7;
    case Qt::Key_F8: return kVK_F8;
    case Qt::Key_F9: return kVK_F9;
    case Qt::Key_F10: return kVK_F10;
    case Qt::Key_F11: return kVK_F11;
    case Qt::Key_F12: return kVK_F12;
    case Qt::Key_Space: return kVK_Space;
    case Qt::Key_Left: return kVK_LeftArrow;
    case Qt::Key_Right: return kVK_RightArrow;
    case Qt::Key_Up: return kVK_UpArrow;
    case Qt::Key_Down: return kVK_DownArrow;
    default: return -1;
    }
}

// On macOS Qt swaps Control/Meta: Qt::ControlModifier is the Command key.
UInt32 carbonModifiers(Qt::KeyboardModifiers mods)
{
    UInt32 native = 0;
    if (mods & Qt::ControlModifier) native |= cmdKey;
    if (mods & Qt::MetaModifier)    native |= controlKey;
    if (mods & Qt::AltModifier)     native |= optionKey;
    if (mods & Qt::ShiftModifier)   native |= shiftKey;
    return native;
}

CGKeyCode rightModKeyCode(GlobalHotkey::ModKey key)
{
    switch (key) {
    case GlobalHotkey::ModKey::RightCmd:   return 0x36; // kVK_RightCommand
    case GlobalHotkey::ModKey::RightShift: return 0x3C;
    case GlobalHotkey::ModKey::RightAlt:   return 0x3D; // right option
    case GlobalHotkey::ModKey::RightCtrl:  return 0x3E;
    }
    return 0;
}

} // namespace

struct GlobalHotkey::Impl
{
    GlobalHotkey *owner = nullptr;

    // combo mode
    EventHotKeyRef hotKeyRef = nullptr;
    EventHandlerRef handlerRef = nullptr;

    // modifier-tap mode
    CFMachPortRef tap = nullptr;
    CFRunLoopSourceRef tapSource = nullptr;
    CGKeyCode tapKeyCode = 0;
    bool tapPending = false; // target modifier is down, no other key seen

    static OSStatus hotKeyCallback(EventHandlerCallRef, EventRef event, void *userData)
    {
        auto *impl = static_cast<Impl *>(userData);
        EventHotKeyID hkID;
        GetEventParameter(event, kEventParamDirectObject, typeEventHotKeyID, nullptr,
                           sizeof(hkID), nullptr, &hkID);
        if (hkID.signature == kHotKeySignature && hkID.id == kHotKeyId)
            emit impl->owner->activated();
        return noErr;
    }

    // Tap-detection: target modifier pressed then released with nothing else
    // in between. Any other keypress or modifier change cancels the pending
    // tap, so holding right-Cmd for a Cmd+C etc never fires.
    static CGEventRef tapCallback(CGEventTapProxy, CGEventType type, CGEventRef event, void *userData)
    {
        auto *impl = static_cast<Impl *>(userData);

        if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
            if (impl->tap)
                CGEventTapEnable(impl->tap, true);
            return event;
        }

        if (type == kCGEventFlagsChanged) {
            const CGKeyCode code = CGKeyCode(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
            if (code == impl->tapKeyCode) {
                // Down or up? Down = some modifier flag newly present.
                // Simplest reliable check: pending toggles on alternate events
                // for this keycode, cancelled by anything else in between.
                if (!impl->tapPending) {
                    impl->tapPending = true;
                } else {
                    impl->tapPending = false;
                    emit impl->owner->activated();
                }
            } else {
                impl->tapPending = false; // some other modifier moved
            }
        } else if (type == kCGEventKeyDown) {
            impl->tapPending = false; // modifier is being used as a chord
        }

        return event; // listen-only: never swallow
    }
};

GlobalHotkey::GlobalHotkey(QObject *parent)
    : QObject(parent)
    , m_impl(new Impl{this})
{
}

GlobalHotkey::~GlobalHotkey()
{
    unregisterHotkey();
    delete m_impl;
}

bool GlobalHotkey::needsAccessibility() const
{
    return m_tapMode && !AXIsProcessTrusted();
}

bool GlobalHotkey::isSupported(const QKeySequence &seq) const
{
    return carbonKeyCode(seq[0].key()) >= 0;
}

bool GlobalHotkey::registerNative()
{
    if (m_tapMode) {
        // Listen-only keyboard tap — needs Accessibility trust.
        if (!AXIsProcessTrusted()) {
            qWarning() << "[hotkey] modifier tap needs Accessibility permission";
            return false;
        }

        m_impl->tapKeyCode = rightModKeyCode(m_modKey);
        m_impl->tapPending = false;

        const CGEventMask mask = CGEventMaskBit(kCGEventFlagsChanged)
                               | CGEventMaskBit(kCGEventKeyDown);
        m_impl->tap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap,
                                       kCGEventTapOptionListenOnly, mask,
                                       &Impl::tapCallback, m_impl);
        if (!m_impl->tap)
            return false;

        m_impl->tapSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, m_impl->tap, 0);
        CFRunLoopAddSource(CFRunLoopGetMain(), m_impl->tapSource, kCFRunLoopCommonModes);
        CGEventTapEnable(m_impl->tap, true);
        return true;
    }

    // Combo mode via Carbon.
    const QKeyCombination combo = m_seq[0];
    const int keyCode = carbonKeyCode(combo.key());
    if (keyCode < 0)
        return false;

    if (!m_impl->handlerRef) {
        EventTypeSpec spec{kEventClassKeyboard, kEventHotKeyPressed};
        InstallApplicationEventHandler(&Impl::hotKeyCallback, 1, &spec,
                                        m_impl, &m_impl->handlerRef);
    }

    const EventHotKeyID hkID{kHotKeySignature, kHotKeyId};
    const OSStatus status = RegisterEventHotKey(UInt32(keyCode),
                                                 carbonModifiers(combo.keyboardModifiers()),
                                                 hkID, GetApplicationEventTarget(), 0,
                                                 &m_impl->hotKeyRef);
    if (status != noErr) {
        m_impl->hotKeyRef = nullptr;
        return false;
    }
    return true;
}

void GlobalHotkey::unregisterNative()
{
    if (m_impl->hotKeyRef) {
        UnregisterEventHotKey(m_impl->hotKeyRef);
        m_impl->hotKeyRef = nullptr;
    }
    if (m_impl->tap) {
        CGEventTapEnable(m_impl->tap, false);
        CFRunLoopRemoveSource(CFRunLoopGetMain(), m_impl->tapSource, kCFRunLoopCommonModes);
        CFRelease(m_impl->tapSource);
        CFRelease(m_impl->tap);
        m_impl->tapSource = nullptr;
        m_impl->tap = nullptr;
    }
    m_impl->tapPending = false;
}

void GlobalHotkey::unregisterHotkey()
{
    unregisterNative();
    if (m_impl->handlerRef) {
        RemoveEventHandler(m_impl->handlerRef);
        m_impl->handlerRef = nullptr;
    }
    m_registered = false;
}
