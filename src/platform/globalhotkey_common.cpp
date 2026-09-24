// Platform-independent half of GlobalHotkey: mode/sequence bookkeeping and
// the suspend/resume dance. The native register/unregister/isSupported live
// in globalhotkey_mac.cpp / globalhotkey_win.cpp.
#include "globalhotkey.h"

QString GlobalHotkey::modKeyLabel(ModKey key)
{
#ifdef Q_OS_MAC
    switch (key) {
    case ModKey::RightCmd:   return QStringLiteral("Right ⌘");
    case ModKey::RightAlt:   return QStringLiteral("Right ⌥");
    case ModKey::RightShift: return QStringLiteral("Right ⇧");
    case ModKey::RightCtrl:  return QStringLiteral("Right ⌃");
    case ModKey::LeftCmd:    return QStringLiteral("Left ⌘");
    case ModKey::LeftAlt:    return QStringLiteral("Left ⌥");
    case ModKey::LeftShift:  return QStringLiteral("Left ⇧");
    case ModKey::LeftCtrl:   return QStringLiteral("Left ⌃");
    }
#else
    switch (key) {
    case ModKey::RightCmd:   return QStringLiteral("Right Win");
    case ModKey::RightAlt:   return QStringLiteral("Right Alt");
    case ModKey::RightShift: return QStringLiteral("Right Shift");
    case ModKey::RightCtrl:  return QStringLiteral("Right Ctrl");
    case ModKey::LeftCmd:    return QStringLiteral("Left Win");
    case ModKey::LeftAlt:    return QStringLiteral("Left Alt");
    case ModKey::LeftShift:  return QStringLiteral("Left Shift");
    case ModKey::LeftCtrl:   return QStringLiteral("Left Ctrl");
    }
#endif
    return QString();
}

QList<GlobalHotkey::ModKey> GlobalHotkey::modKeys()
{
#ifdef Q_OS_MAC
    return {ModKey::RightCmd, ModKey::RightAlt, ModKey::RightShift, ModKey::RightCtrl,
            ModKey::LeftCmd,  ModKey::LeftAlt,  ModKey::LeftShift,  ModKey::LeftCtrl};
#else
    // Left Win opens Start and Left Alt focuses the menu bar on a lone tap,
    // and the hook only listens, so neither is offered on Windows.
    return {ModKey::RightCmd, ModKey::RightAlt, ModKey::RightShift, ModKey::RightCtrl,
            ModKey::LeftShift, ModKey::LeftCtrl};
#endif
}

QString GlobalHotkey::comboLabel() const
{
    return m_tapMode ? modKeyLabel(m_modKey)
                     : m_seq.toString(QKeySequence::NativeText);
}

bool GlobalHotkey::applyCurrent()
{
    if (m_suspended)
        return true; // stored; resume() registers

    unregisterNative();
    m_registered = registerNative();
    return m_registered;
}

bool GlobalHotkey::setSequence(const QKeySequence &seq)
{
    if (seq.isEmpty() || !isSupported(seq))
        return false;

    const QKeySequence oldSeq = m_seq;
    const bool oldTap = m_tapMode;

    m_seq = seq;
    m_tapMode = false;
    if (applyCurrent())
        return true;

    // Roll back so a rejected combo doesn't leave the user with nothing.
    m_seq = oldSeq;
    m_tapMode = oldTap;
    applyCurrent();
    return false;
}

bool GlobalHotkey::setModifierTap(ModKey key)
{
    const ModKey oldKey = m_modKey;
    const bool oldTap = m_tapMode;

    m_modKey = key;
    m_tapMode = true;
    if (applyCurrent())
        return true;

    m_modKey = oldKey;
    m_tapMode = oldTap;
    applyCurrent();
    return false;
}

void GlobalHotkey::suspend()
{
    if (m_suspended)
        return;
    unregisterNative();
    m_registered = false;
    m_suspended = true;
}

bool GlobalHotkey::resume()
{
    if (!m_suspended)
        return m_registered;
    m_suspended = false;
    m_registered = registerNative();
    return m_registered;
}
