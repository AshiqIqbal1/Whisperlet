#include "overlaywindow.h"

#include <QWidget>

// windows.h defines min/max macros that break std::min and friends.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {

void applyOverlayStyle(QWidget *widget)
{
    if (!widget)
        return;
    widget->winId(); // force native window creation
    HWND hwnd = reinterpret_cast<HWND>(widget->winId());
    if (!hwnd)
        return;

    // WS_EX_NOACTIVATE: showing or clicking it never takes focus from the
    // field being dictated into. WS_EX_TOOLWINDOW keeps it off the taskbar
    // and out of Alt+Tab.
    const LONG_PTR ex = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

} // namespace

void OverlayWindow::configure(QWidget *widget)
{
    applyOverlayStyle(widget);
}

bool OverlayWindow::recreateBeforeShow()
{
    // Windows virtual desktops: a window belongs to the desktop it was
    // created on and stays there. There is no public API to put one on all
    // desktops, so a pill created at startup would stay on desktop 1 and be
    // invisible when dictating on desktop 2. A freshly created window lands
    // on whichever desktop is current, so the caller rebuilds it each show.
    return true;
}

void OverlayWindow::beforeShow(QWidget *)
{
    // Nothing to do: the caller has just discarded the native window, and
    // afterShow styles the new one.
}

void OverlayWindow::afterShow(QWidget *widget)
{
    // The window above is brand new, so its style has to be set again, and
    // topmost re-asserted so it sits over the application being typed into.
    applyOverlayStyle(widget);
}
