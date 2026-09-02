#ifndef GLOBALHOTKEY_H
#define GLOBALHOTKEY_H

#include <QKeySequence>
#include <QObject>

// Cross-platform "works even when the app isn't focused" hotkey, with two
// trigger styles:
//
//  1. Key combination ("Ctrl+Shift+R") — Carbon RegisterEventHotKey on
//     macOS, Win32 RegisterHotKey on Windows.
//  2. Single modifier TAP (press+release right-Cmd alone, OpenSuperWhisper
//     style) — bare modifiers can't be registered as hotkeys, so this uses
//     a listen-only CGEventTap on macOS (needs Accessibility permission,
//     same one dictation uses) and a WH_KEYBOARD_LL hook on Windows.
//
// setSequence()/setModifierTap() switch the mode. While suspended (shortcut
// editor open) changes are stored and registration is deferred to resume().
class GlobalHotkey : public QObject
{
    Q_OBJECT

public:
    enum class ModKey {
        RightCmd,   // ⌘ on mac, Win key on Windows
        RightAlt,   // ⌥ on mac
        RightShift,
        RightCtrl,
    };

    explicit GlobalHotkey(QObject *parent = nullptr);
    ~GlobalHotkey() override;

    static QKeySequence defaultSequence() { return QKeySequence(QStringLiteral("Ctrl+Shift+R")); }
    static QString modKeyLabel(ModKey key);

    bool setSequence(const QKeySequence &seq);
    bool setModifierTap(ModKey key);

    // True when the current trigger is actually registered with the OS.
    bool isActive() const { return m_registered; }

    // Modifier-tap needs the Accessibility permission on macOS; this says
    // whether that is the thing standing in the way.
    bool needsAccessibility() const;

    bool isModifierTapMode() const { return m_tapMode; }
    QKeySequence sequence() const { return m_seq; }
    ModKey modifierKey() const { return m_modKey; }

    // Native-looking label for the current trigger, e.g. "⌘⇧R" or "Right ⌥".
    QString comboLabel() const;

    void suspend();
    bool resume(); // false if the stored trigger failed to re-register

    void unregisterHotkey();

signals:
    void activated();

private:
    // Backends: register/unregister whatever the current mode says.
    bool registerNative();
    void unregisterNative();
    bool isSupported(const QKeySequence &seq) const;
    bool applyCurrent(); // shared bookkeeping around registerNative()

    struct Impl;
    Impl *m_impl = nullptr;
    QKeySequence m_seq;
    ModKey m_modKey = ModKey::RightCmd;
    bool m_tapMode = false;
    bool m_registered = false;
    bool m_suspended = false;
};

#endif // GLOBALHOTKEY_H
