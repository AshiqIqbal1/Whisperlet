#include "overlaywindow.h"

#include <QWidget>

#import <AppKit/AppKit.h>

void OverlayWindow::makeFloatingOverlay(QWidget *widget)
{
    if (!widget)
        return;

    // Force native window creation before reaching for it.
    widget->winId();

    NSView *view = reinterpret_cast<NSView *>(widget->winId());
    NSWindow *window = [view window];
    if (!window)
        return;

    // CanJoinAllSpaces: follow the user to whichever desktop they are on.
    // FullScreenAuxiliary: allowed to draw over a fullscreen app instead of
    // being trapped behind it on its own Space.
    // Stationary: don't slide around during Space switch animations.
    [window setCollectionBehavior:(NSWindowCollectionBehaviorCanJoinAllSpaces
                                   | NSWindowCollectionBehaviorFullScreenAuxiliary
                                   | NSWindowCollectionBehaviorStationary
                                   | NSWindowCollectionBehaviorIgnoresCycle)];

    // Above normal and floating windows, at the level menus use, so a
    // fullscreen app cannot cover it.
    [window setLevel:NSPopUpMenuWindowLevel];

    // Never become key or main: the field being dictated into keeps focus.
    [window setHidesOnDeactivate:NO];
}
