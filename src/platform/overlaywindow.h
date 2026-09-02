#ifndef OVERLAYWINDOW_H
#define OVERLAYWINDOW_H

class QWidget;

// Makes a window behave like a system overlay rather than an ordinary app
// window: visible on every Space, over fullscreen apps, and never taking
// focus. Without this the recording pill only appears on whichever Space
// the app itself lives on, so it is invisible when you are dictating into
// a fullscreen terminal or another desktop.
namespace OverlayWindow {

void makeFloatingOverlay(QWidget *widget);

} // namespace OverlayWindow

#endif // OVERLAYWINDOW_H
