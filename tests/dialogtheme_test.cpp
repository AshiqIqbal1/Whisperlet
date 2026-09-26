// Checks the pieces of the UI the dark stylesheet used to leave to the native
// style stay readable when macOS is in Light mode (#71):
//  - a QMessageBox parented to the main window (the Accessibility prompt)
//    paints an opaque dark window and visible button chrome, instead of
//    showing the native (white in Light) window behind light text;
//  - Theme::pinDarkColorScheme() overrides a Light system appearance, so
//    native radio button and check box indicators are drawn for Dark.
// The stylesheet checks run anywhere. The colour scheme checks only run on
// a platform that honours colour scheme requests (Cocoa, not offscreen) on
// Qt 6.8+, so run this binary with QT_QPA_PLATFORM=cocoa to cover them too.

#include "theme.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QImage>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QStyleHints>
#include <QVBoxLayout>

#include <cstdio>

namespace {
int failures = 0;

// Background the stylesheet gives every dialog.
const QColor kDialogBg{0x1A, 0x1A, 0x1D};

void check(bool condition, const char *what)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

// The widget as drawn on screen, at one pixel per logical pixel so widget
// coordinates index straight into it on a Retina display too.
QImage grab(QWidget &w)
{
    w.show();
    QApplication::processEvents();
    return w.grab().toImage().scaled(w.size());
}

// Largest per-channel distance from the dialog background inside `rect`.
// A native indicator drawn for the Light appearance on the dark background
// is almost black and stays within a few steps of it (3 on macOS 26, against
// about 22 once drawn for Dark).
int maxContrast(const QImage &img, const QRect &rect)
{
    int best = 0;
    for (int y = rect.top(); y <= rect.bottom(); ++y) {
        for (int x = rect.left(); x <= rect.right(); ++x) {
            const QColor c = img.pixelColor(x, y);
            best = qMax(best, qAbs(c.red() - kDialogBg.red()));
            best = qMax(best, qAbs(c.green() - kDialogBg.green()));
            best = qMax(best, qAbs(c.blue() - kDialogBg.blue()));
        }
    }
    return best;
}

// Where the unchecked indicator of a radio button or check box is drawn:
// the square at the left end of the widget, in the dialog's coordinates.
QRect indicatorRect(const QWidget &button)
{
    const int side = qMin(button.height(), 16);
    return QRect(button.mapToParent(QPoint(0, (button.height() - side) / 2)),
                 QSize(side, side));
}
} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    // Stand-in for MainWindow: the prompt only gets the stylesheet by
    // inheriting it from its parent.
    QWidget host;
    host.setStyleSheet(Theme::styleSheet());

    // The Accessibility prompt as promptForAccessibility() builds it.
    {
        QMessageBox box(&host);
        box.setIcon(QMessageBox::Information);
        box.setText(QStringLiteral("Whisperlet needs Accessibility access to type into other apps."));
        box.addButton(QStringLiteral("Open Settings"), QMessageBox::AcceptRole);
        box.addButton(QStringLiteral("Later"), QMessageBox::RejectRole);
        const QImage img = grab(box);

        check(img.pixelColor(3, 3) == kDialogBg, "message box paints the dark dialog background");

        const QList<QPushButton *> buttons = box.findChildren<QPushButton *>();
        check(buttons.size() == 2, "message box has its two buttons");
        for (const QPushButton *button : buttons) {
            const QPoint inside = button->mapTo(&box, QPoint(6, button->height() / 2));
            check(img.pixelColor(inside) != kDialogBg, "message box button draws its own chrome");
        }
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    // The colour scheme pin. Ask for Light first, like a Light system
    // appearance would; if the platform ignores the request it cannot
    // honour the pin either, so there is nothing to check.
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
    if (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Light) {
        Theme::pinDarkColorScheme();
        check(QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark,
              "pinDarkColorScheme overrides a Light appearance");

        QDialog dlg(&host);
        auto *layout = new QVBoxLayout(&dlg);
        auto *radio = new QRadioButton(QStringLiteral("Unchecked radio"), &dlg);
        auto *box = new QCheckBox(QStringLiteral("Unchecked box"), &dlg);
        layout->addWidget(radio);
        layout->addWidget(box);
        const QImage img = grab(dlg);

        check(maxContrast(img, indicatorRect(*radio)) > 10, "unchecked radio indicator is visible");
        check(maxContrast(img, indicatorRect(*box)) > 10, "unchecked check box indicator is visible");
    } else {
        std::printf("Platform ignores colour scheme requests, skipping those checks\n");
    }
#endif

    if (failures > 0) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("All dialogtheme checks passed\n");
    return 0;
}
