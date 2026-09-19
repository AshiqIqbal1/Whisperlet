#include "textinjector.h"
#include "textinjector_mac_chunking.h"

#include <QDesktopServices>
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
    // Type the characters directly via a synthetic keyboard event carrying
    // Unicode text, rather than round-tripping through the system clipboard
    // and Cmd+V. This avoids clobbering (and having to restore) whatever the
    // user had copied, and removes the fixed delays the clipboard approach
    // needed to let the pasteboard sync and the paste land.
    if (text.isEmpty())
        return;

    // CGEventKeyboardSetUnicodeString only carries a small number of UniChars
    // per event reliably, so long dictated passages are sent in chunks
    // rather than one event per character (which the Windows SendInput path
    // avoids) or a single oversized event (which the API does not support).
    constexpr int kChunk = 20;
    const int length = text.length();
    const ushort *utf16 = text.utf16();

    for (int i = 0; i < length;) {
        const int count = textInjectorMacNextChunkLength(utf16, length, i, kChunk);

        CGEventRef keyDown = CGEventCreateKeyboardEvent(nullptr, 0, true);
        CGEventRef keyUp = CGEventCreateKeyboardEvent(nullptr, 0, false);

        CGEventKeyboardSetUnicodeString(keyDown, UniCharCount(count),
                                         reinterpret_cast<const UniChar *>(utf16 + i));
        CGEventKeyboardSetUnicodeString(keyUp, UniCharCount(count),
                                         reinterpret_cast<const UniChar *>(utf16 + i));

        CGEventPost(kCGHIDEventTap, keyDown);
        CGEventPost(kCGHIDEventTap, keyUp);

        CFRelease(keyDown);
        CFRelease(keyUp);

        i += count;
    }
}
