#include "overlaywindow.h"

#include <QDebug>
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

    // Above fullscreen windows. A fullscreen app's own window sits high, so
    // menu level is not enough to float over another app's fullscreen Space.
    [window setLevel:NSScreenSaverWindowLevel];

    // A plain panel still wants to activate its app, which macOS answers by
    // switching Spaces away from the fullscreen app the user is typing in.
    // A non-activating floating panel can be shown over that Space instead.
    if ([window isKindOfClass:[NSPanel class]]) {
        NSPanel *panel = static_cast<NSPanel *>(window);
        [panel setStyleMask:([panel styleMask] | NSWindowStyleMaskNonactivatingPanel)];
        [panel setFloatingPanel:YES];
        [panel setBecomesKeyOnlyIfNeeded:YES];
        [panel setWorksWhenModal:YES];
    }

    // Never become key or main: the field being dictated into keeps focus.
    [window setHidesOnDeactivate:NO];

    qInfo() << "[overlay] collectionBehavior=" << quint64([window collectionBehavior])
            << "level=" << int([window level]);
}
