#ifndef TEXTINJECTOR_H
#define TEXTINJECTOR_H

#include <QString>

// Dictation output: synthesize keystrokes to type text directly into
// whichever app currently has focus, without touching the system
// clipboard. Platform backends:
//   mac — CGEventKeyboardSetUnicodeString; requires the Accessibility permission
//   win — SendInput with KEYEVENTF_UNICODE; no permission needed
namespace TextInjector {

// Can we synthesize keystrokes right now? (mac: Accessibility granted)
bool canInject();

// Trigger the OS permission flow if there is one (mac shows the prompt).
// No-op on Windows.
void requestPermission();

// Jump the user straight to the relevant settings pane (macOS: Privacy &
// Security -> Accessibility). No-op on Windows.
void openPermissionSettings();

// Types the text via synthetic keystrokes. Call only when canInject() is
// true. The system clipboard is never touched.
void pasteIntoActiveApp(const QString &text);

} // namespace TextInjector

#endif // TEXTINJECTOR_H
