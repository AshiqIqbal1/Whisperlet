#include "overlaywindow.h"

#include <QWidget>

// windows.h defines min/max macros that break std::min and friends.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void OverlayWindow::makeFloatingOverlay(QWidget *widget)
{
    if (!widget)
        return;

    widget->winId(); // force native window creation
    HWND hwnd = reinterpret_cast<HWND>(widget->winId());
    if (!hwnd)
        return;

    // WS_EX_NOACTIVATE: clicking or showing it never takes focus from the
    // field being dictated into. WS_EX_TOOLWINDOW keeps it off the taskbar
    // and out of Alt+Tab. Windows has no Spaces, so topmost is enough.
    LONG_PTR ex = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}
