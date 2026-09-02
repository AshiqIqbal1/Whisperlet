#include "textinjector.h"

#include <QString>
#include <QVector>

#include <windows.h>

#include <algorithm>
#include <vector>

bool TextInjector::canInject()
{
    return true; // SendInput needs no special permission
}

void TextInjector::requestPermission()
{
    // nothing to do on Windows
}

void TextInjector::openPermissionSettings()
{
    // Windows needs no permission for SendInput.
}

void TextInjector::pasteIntoActiveApp(const QString &text)
{
    // Type the characters directly rather than going through the clipboard
    // and Ctrl+V. Three reasons:
    //  - the clipboard is never touched, so the user's copied content is
    //    left alone and the transcript never sits in a shared buffer
    //  - Ctrl+V is not paste everywhere: the classic console and several
    //    terminals use something else, and the keystroke would be lost
    //  - no timing race between setting the clipboard and the paste landing
    //
    // KEYEVENTF_UNICODE carries the character itself, so keyboard layout
    // does not matter either.
    const QVector<uint> ucs4 = text.toUcs4();
    if (ucs4.isEmpty())
        return;

    std::vector<INPUT> inputs;
    inputs.reserve(size_t(ucs4.size()) * 4);

    auto appendUnit = [&inputs](wchar_t unit) {
        INPUT down{};
        down.type = INPUT_KEYBOARD;
        down.ki.wScan = unit;
        down.ki.dwFlags = KEYEVENTF_UNICODE;
        inputs.push_back(down);

        INPUT up = down;
        up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        inputs.push_back(up);
    };

    for (uint cp : ucs4) {
        if (cp > 0xFFFF) {
            // Outside the BMP: send the surrogate pair, e.g. emoji.
            const uint v = cp - 0x10000;
            appendUnit(wchar_t(0xD800 + (v >> 10)));
            appendUnit(wchar_t(0xDC00 + (v & 0x3FF)));
        } else {
            appendUnit(wchar_t(cp));
        }
    }

    // Send in chunks: a very long transcript in one call can be dropped by
    // applications that process input synchronously.
    constexpr size_t kChunk = 200;
    for (size_t i = 0; i < inputs.size(); i += kChunk) {
        const UINT count = UINT(std::min(kChunk, inputs.size() - i));
        SendInput(count, inputs.data() + i, sizeof(INPUT));
    }
}
