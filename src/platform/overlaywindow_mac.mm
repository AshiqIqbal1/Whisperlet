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

    // Set the level AFTER the style mask: changing a panel's style mask
    // resets its level back to the panel default, which silently undid this.
    // Status level floats over other apps' fullscreen windows without
    // fighting system UI the way screen saver level would.
    [window setLevel:NSStatusWindowLevel];

    // Never become key or main: the field being dictated into keeps focus.
    [window setHidesOnDeactivate:NO];
}
