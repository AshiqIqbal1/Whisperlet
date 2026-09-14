#ifndef OVERLAYWINDOW_H
#define OVERLAYWINDOW_H

class QWidget;

// Makes a window behave like a system overlay rather than an ordinary app
// window: visible wherever the user currently is, above other applications,
// and never taking focus.
//
// Split into three calls because the timing matters. Window style has to be
// set once, before the window is ever put on screen, since changing it later
// makes the OS rebuild the window and reassign which desktop it belongs to.
// The other two are cheap and run on every show.
namespace OverlayWindow {

// One time setup. Call before the window is first shown.
void configure(QWidget *widget);

// Called immediately before each show, while the window is still hidden.
void beforeShow(QWidget *widget);

// Called immediately after each show.
void afterShow(QWidget *widget);

} // namespace OverlayWindow

#endif // OVERLAYWINDOW_H
