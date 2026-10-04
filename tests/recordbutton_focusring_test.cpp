// The record button keeps focus at launch and after a click so Space
// records, which drew a blue focus ring around it all the time. The ring
// is now only for keyboard navigation (Tab). Renders the button and counts
// accent-blue pixels for each way of getting focus.

#include "recordbutton.h"
#include "theme.h"

#include <QApplication>
#include <QFocusEvent>
#include <QImage>
#include <QTest>
#include <QWidget>

#include <cstdio>
#include <cstdlib>

namespace {
int failures = 0;

void check(bool condition, const char *what)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

int accentPixels(RecordButton &button)
{
    const QImage img = button.grab().toImage();
    const QColor accent = Theme::Accent;
    int count = 0;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            if (qAbs(c.red() - accent.red()) < 40 && qAbs(c.green() - accent.green()) < 40
                && qAbs(c.blue() - accent.blue()) < 40 && c.alpha() > 128)
                ++count;
        }
    }
    return count;
}

void giveFocus(RecordButton &button, Qt::FocusReason reason)
{
    button.clearFocus();
    button.setFocus(reason);
    QCoreApplication::processEvents();
}
} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QWidget window;
    RecordButton button(&window);
    button.resize(button.sizeHint());
    window.resize(button.sizeHint());
    window.show();
    window.activateWindow();
    QTest::qWaitForWindowActive(&window);

    giveFocus(button, Qt::OtherFocusReason); // launch: MainWindow::setFocus()
    check(accentPixels(button) == 0, "no ring when focused programmatically at launch");

    giveFocus(button, Qt::MouseFocusReason);
    check(accentPixels(button) == 0, "no ring after clicking the button");

    giveFocus(button, Qt::TabFocusReason);
    check(accentPixels(button) > 0, "ring shows when reached with Tab");

    check(button.hasFocus(), "button really has focus");
    giveFocus(button, Qt::MouseFocusReason);
    check(accentPixels(button) == 0, "ring goes away once focus moves by mouse");

    if (failures == 0)
        std::printf("All record button focus ring tests passed.\n");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
