#include "textinjector.h"

#include <QGuiApplication>
#include <QClipboard>
#include <QTimer>

#include <windows.h>

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
    const QString previous = QGuiApplication::clipboard()->text();
    QGuiApplication::clipboard()->setText(text);

    // Put whatever the user had copied back once the paste has landed.
    if (!previous.isEmpty()) {
        QTimer::singleShot(1000, [previous] {
            QGuiApplication::clipboard()->setText(previous);
        });
    }

    QTimer::singleShot(150, [] {
        INPUT inputs[4] = {};

        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = VK_CONTROL;

        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = 'V';

        inputs[2].type = INPUT_KEYBOARD;
        inputs[2].ki.wVk = 'V';
        inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

        inputs[3].type = INPUT_KEYBOARD;
        inputs[3].ki.wVk = VK_CONTROL;
        inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

        SendInput(4, inputs, sizeof(INPUT));
    });
}
