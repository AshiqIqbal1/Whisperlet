#include "textinjector.h"

#include <QDesktopServices>
#include <QGuiApplication>
#include <QClipboard>
#include <QTimer>
#include <QUrl>

#include <ApplicationServices/ApplicationServices.h>

bool TextInjector::canInject()
{
    return AXIsProcessTrusted();
}

void TextInjector::requestPermission()
{
    // Shows the system prompt and lists the app in
    // System Settings -> Privacy & Security -> Accessibility.
    const void *keys[] = {kAXTrustedCheckOptionPrompt};
    const void *values[] = {kCFBooleanTrue};
    CFDictionaryRef options = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1,
                                                 &kCFTypeDictionaryKeyCallBacks,
                                                 &kCFTypeDictionaryValueCallBacks);
    AXIsProcessTrustedWithOptions(options);
    CFRelease(options);
}

void TextInjector::openPermissionSettings()
{
    // Deep-links to Privacy & Security -> Accessibility. Same URL scheme
    // works on Ventura/Sonoma and later System Settings.
    QDesktopServices::openUrl(QUrl(QStringLiteral(
        "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility")));
}

void TextInjector::pasteIntoActiveApp(const QString &text)
{
    const QString previous = QGuiApplication::clipboard()->text();
    QGuiApplication::clipboard()->setText(text);

    // Give the pasteboard a beat to sync before we fire Cmd+V, otherwise the
    // target app occasionally pastes the previous clipboard contents.
    QTimer::singleShot(150, [] {
        const CGKeyCode kVK_V = 9; // kVK_ANSI_V

        CGEventRef vDown = CGEventCreateKeyboardEvent(nullptr, kVK_V, true);
        CGEventRef vUp = CGEventCreateKeyboardEvent(nullptr, kVK_V, false);
        CGEventSetFlags(vDown, kCGEventFlagMaskCommand);
        CGEventSetFlags(vUp, kCGEventFlagMaskCommand);

        CGEventPost(kCGHIDEventTap, vDown);
        CGEventPost(kCGHIDEventTap, vUp);

        CFRelease(vDown);
        CFRelease(vUp);
    });

    // Put whatever the user had copied back once the paste has landed.
    if (!previous.isEmpty()) {
        QTimer::singleShot(1000, [previous] {
            QGuiApplication::clipboard()->setText(previous);
        });
    }
}
