// Windows backend for GlobalHotkey.
//
// Combo mode: Win32 RegisterHotKey + a Qt native event filter for WM_HOTKEY.
// Modifier-tap mode: bare modifiers can't be registered as hotkeys, so a
// WH_KEYBOARD_LL low-level hook watches for the target modifier being
// pressed and released with no other key in between. No permission needed.
#include "globalhotkey.h"

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>

#include <windows.h>

namespace {

constexpr int kHotKeyId = 0x5746; // 'WF'

int winVirtualKey(Qt::Key key)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return 'A' + (key - Qt::Key_A);
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
        return '0' + (key - Qt::Key_0);
    if (key >= Qt::Key_F1 && key <= Qt::Key_F12)
        return VK_F1 + (key - Qt::Key_F1);

    switch (key) {
    case Qt::Key_Space: return VK_SPACE;
    case Qt::Key_Left:  return VK_LEFT;
    case Qt::Key_Right: return VK_RIGHT;
    case Qt::Key_Up:    return VK_UP;
    case Qt::Key_Down:  return VK_DOWN;
    default: return -1;
    }
}

UINT winModifiers(Qt::KeyboardModifiers mods)
{
    UINT native = MOD_NOREPEAT;
    if (mods & Qt::ControlModifier) native |= MOD_CONTROL;
    if (mods & Qt::ShiftModifier)   native |= MOD_SHIFT;
    if (mods & Qt::AltModifier)     native |= MOD_ALT;
    if (mods & Qt::MetaModifier)    native |= MOD_WIN;
    return native;
}

DWORD rightModVk(GlobalHotkey::ModKey key)
{
    switch (key) {
    case GlobalHotkey::ModKey::RightCmd:   return VK_RWIN;
    case GlobalHotkey::ModKey::RightAlt:   return VK_RMENU;
    case GlobalHotkey::ModKey::RightShift: return VK_RSHIFT;
    case GlobalHotkey::ModKey::RightCtrl:  return VK_RCONTROL;
    }
    return 0;
}

} // namespace

struct GlobalHotkey::Impl : public QAbstractNativeEventFilter
{
    GlobalHotkey *owner = nullptr;
    bool comboRegistered = false;
    bool filterInstalled = false;

    HHOOK hook = nullptr;
    DWORD tapVk = 0;
    bool tapPending = false;

    // WH_KEYBOARD_LL callbacks get no user pointer — single-instance static.
    static Impl *s_instance;

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *) override
    {
        if (eventType != "windows_generic_MSG")
            return false;
        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_HOTKEY && msg->wParam == kHotKeyId) {
            emit owner->activated();
            return true;
        }
        return false;
    }

    static LRESULT CALLBACK hookProc(int code, WPARAM wParam, LPARAM lParam)
    {
        if (code == HC_ACTION && s_instance && s_instance->hook) {
            auto *kb = reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);
            const bool down = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
            const bool up = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);

            if (kb->vkCode == s_instance->tapVk) {
                if (down) {
                    s_instance->tapPending = true;
                } else if (up && s_instance->tapPending) {
                    s_instance->tapPending = false;
                    // Hook runs on the GUI thread's message loop, but keep
                    // the emission out of the hook callback itself.
                    QMetaObject::invokeMethod(s_instance->owner, "activated",
                                              Qt::QueuedConnection);
                }
            } else if (down) {
                s_instance->tapPending = false; // chorded with something else
            }
        }
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }
};

GlobalHotkey::Impl *GlobalHotkey::Impl::s_instance = nullptr;

GlobalHotkey::GlobalHotkey(QObject *parent)
    : QObject(parent)
    , m_impl(new Impl)
{
    m_impl->owner = this;
    Impl::s_instance = m_impl;
}

GlobalHotkey::~GlobalHotkey()
{
    unregisterHotkey();
    if (m_impl->filterInstalled && QCoreApplication::instance())
        QCoreApplication::instance()->removeNativeEventFilter(m_impl);
    Impl::s_instance = nullptr;
    delete m_impl;
}

bool GlobalHotkey::needsAccessibility() const
{
    return false; // Windows needs no permission for the low level hook
}

bool GlobalHotkey::isSupported(const QKeySequence &seq) const
{
    return winVirtualKey(seq[0].key()) >= 0;
}

bool GlobalHotkey::registerNative()
{
    if (m_tapMode) {
        m_impl->tapVk = rightModVk(m_modKey);
        m_impl->tapPending = false;
        m_impl->hook = SetWindowsHookExW(WH_KEYBOARD_LL, &Impl::hookProc,
                                         GetModuleHandleW(nullptr), 0);
        return m_impl->hook != nullptr;
    }

    const QKeyCombination combo = m_seq[0];
    const int vk = winVirtualKey(combo.key());
    if (vk < 0)
        return false;

    if (!RegisterHotKey(nullptr, kHotKeyId, winModifiers(combo.keyboardModifiers()), UINT(vk)))
        return false;
    m_impl->comboRegistered = true;

    if (!m_impl->filterInstalled) {
        QCoreApplication::instance()->installNativeEventFilter(m_impl);
        m_impl->filterInstalled = true;
    }
    return true;
}

void GlobalHotkey::unregisterNative()
{
    if (m_impl->comboRegistered) {
        UnregisterHotKey(nullptr, kHotKeyId);
        m_impl->comboRegistered = false;
    }
    if (m_impl->hook) {
        UnhookWindowsHookEx(m_impl->hook);
        m_impl->hook = nullptr;
    }
    m_impl->tapPending = false;
}

void GlobalHotkey::unregisterHotkey()
{
    unregisterNative();
    m_registered = false;
}
