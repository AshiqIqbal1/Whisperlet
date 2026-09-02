#ifndef TEXTINJECTOR_H
#define TEXTINJECTOR_H

#include <QString>

// Dictation output: place text on the clipboard and synthesize a paste
// keystroke into whichever app currently has focus. Platform backends:
//   mac — CGEvent Cmd+V; requires the Accessibility permission
//   win — SendInput Ctrl+V; no permission needed
namespace TextInjector {

// Can we synthesize keystrokes right now? (mac: Accessibility granted)
bool canInject();

// Trigger the OS permission flow if there is one (mac shows the prompt).
// No-op on Windows.
void requestPermission();

// Jump the user straight to the relevant settings pane (macOS: Privacy &
// Security -> Accessibility). No-op on Windows.
void openPermissionSettings();

// Clipboard-set + paste keystroke. Call only when canInject() is true.
// The previous clipboard text is restored ~1s after the paste, so
// dictation doesn't clobber whatever the user had copied.
void pasteIntoActiveApp(const QString &text);

} // namespace TextInjector

#endif // TEXTINJECTOR_H
