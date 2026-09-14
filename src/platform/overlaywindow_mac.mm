#include "overlaywindow.h"

#include <QWidget>

#import <AppKit/AppKit.h>

namespace {

NSWindow *nativeWindow(QWidget *widget)
{
    if (!widget)
        return nil;
    widget->winId(); // force native window creation before reaching for it
    NSView *view = reinterpret_cast<NSView *>(widget->winId());
    return [view window];
}

// CanJoinAllSpaces: follow the user to whichever desktop they are on.
// FullScreenAuxiliary: allowed to draw over a fullscreen app rather than
// being trapped behind it on its own Space.
// Stationary: do not slide around during Space switch animations.
constexpr NSWindowCollectionBehavior kBehaviour =
    NSWindowCollectionBehaviorCanJoinAllSpaces
    | NSWindowCollectionBehaviorFullScreenAuxiliary
    | NSWindowCollectionBehaviorStationary
    | NSWindowCollectionBehaviorIgnoresCycle;

} // namespace

void OverlayWindow::configure(QWidget *widget)
{
    NSWindow *window = nativeWindow(widget);
    if (!window)
        return;

    // A plain panel still wants to activate its app, and macOS answers that
    // by switching away from the fullscreen Space the user is typing in. A
    // non activating floating panel can be shown over that Space instead.
    //
    // This runs once and only while the window is hidden. Changing a style
    // mask on a window that is already on screen makes AppKit rebuild it,
    // and the rebuilt window is reassigned to whichever desktop is active,
    // which is exactly the bug this code exists to avoid.
    if ([window isKindOfClass:[NSPanel class]]) {
        NSPanel *panel = static_cast<NSPanel *>(window);
        [panel setStyleMask:([panel styleMask] | NSWindowStyleMaskNonactivatingPanel)];
        [panel setFloatingPanel:YES];
        [panel setBecomesKeyOnlyIfNeeded:YES];
        [panel setWorksWhenModal:YES];
    }

    // Never become key or main: the field being dictated into keeps focus.
    [window setHidesOnDeactivate:NO];

    [window setCollectionBehavior:kBehaviour];
    // Above the menu bar level, which is what lets it draw over another
    // application's fullscreen Space.
    [window setLevel:NSPopUpMenuWindowLevel];
}

bool OverlayWindow::recreateBeforeShow()
{
    return false; // Spaces membership is handled by collection behaviour
}

void OverlayWindow::beforeShow(QWidget *widget)
{
    // Assign the desktop membership while still hidden, so the window is
    // put on screen already belonging to every Space.
    NSWindow *window = nativeWindow(widget);
    if (window)
        [window setCollectionBehavior:kBehaviour];
}

void OverlayWindow::afterShow(QWidget *widget)
{
    // Qt can reset these when it maps the window, so assert them again.
    // Deliberately no style mask here: see configure().
    NSWindow *window = nativeWindow(widget);
    if (!window)
        return;
    [window setCollectionBehavior:kBehaviour];
    [window setLevel:NSPopUpMenuWindowLevel];
}
