// Builds the input-device picker menu the way MainWindow's mic button does
// (same actions, same global stylesheet) and checks it renders inside a
// panel with every device name contained in it. Regression cover for #27,
// where the sheet's transparent QWidget background applied to QMenu too and
// the names ended up drawn over the desktop with no panel behind them.
// Requires QT_QPA_PLATFORM=offscreen (set by the test runner).

#include "theme.h"

#include <QAction>
#include <QApplication>
#include <QFontMetrics>
#include <QImage>
#include <QMenu>

#include <cstdio>

namespace {
int failures = 0;

// Left/right padding QMenu::item reserves in the stylesheet: the check
// column plus the trailing gap. Text has to fit in what is left.
constexpr int kItemPadding = 26 + 14;

void check(bool condition, const char *what)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

// The menu MainWindow pops up under the mic button: "System default" plus
// one checkable entry per input device.
void populate(QMenu &menu, const QStringList &devices, bool systemDefaultChecked)
{
    auto *systemDefault = menu.addAction(QStringLiteral("System default"));
    systemDefault->setCheckable(true);
    systemDefault->setChecked(systemDefaultChecked);

    for (const QString &name : devices) {
        auto *act = menu.addAction(name);
        act->setCheckable(true);
    }
}

QImage render(QMenu &menu)
{
    menu.ensurePolished();
    menu.resize(menu.sizeHint());
    QImage img(menu.size(), QImage::Format_ARGB32);
    img.fill(Qt::transparent); // anything the menu leaves unpainted stays clear
    menu.render(&img);
    return img;
}
} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    qApp->setStyleSheet(Theme::styleSheet());

    const QStringList devices{
        QStringLiteral("MacBook Pro Microphone"),
        QStringLiteral("Microsoft Teams Audio"),
        QStringLiteral("Absurdly Long Virtual Conferencing Input Device Name 2")};

    // The panel itself: the menu has to paint an opaque background under its
    // items, otherwise the device names float over whatever is behind it.
    {
        QMenu menu;
        populate(menu, devices, true);
        const QImage img = render(menu);

        bool opaque = true;
        for (int y : {menu.height() / 4, menu.height() / 2, 3 * menu.height() / 4}) {
            for (int x : {menu.width() / 4, menu.width() / 2, 3 * menu.width() / 4}) {
                if (qAlpha(img.pixel(x, y)) != 255)
                    opaque = false;
            }
        }
        check(opaque, "menu paints an opaque panel behind its items");
    }

    // Every name stays inside the panel, however long the device name is:
    // the menu sizes itself to its widest item.
    {
        QMenu menu;
        populate(menu, devices, true);
        menu.ensurePolished();
        menu.resize(menu.sizeHint());

        const QFontMetrics fm(menu.font());
        bool contained = true;
        for (QAction *act : menu.actions()) {
            if (!menu.rect().contains(menu.actionGeometry(act)))
                contained = false;
            if (fm.horizontalAdvance(act->text()) + kItemPadding > menu.width())
                contained = false;
        }
        check(contained, "every item's text fits inside the panel");
    }

    // The selection checkmark still draws: the checked entry differs from the
    // unchecked one in the check column the item padding reserves.
    {
        QMenu checkedMenu;
        populate(checkedMenu, devices, true);
        const QImage checkedImg = render(checkedMenu);

        QMenu uncheckedMenu;
        populate(uncheckedMenu, devices, false);
        const QImage uncheckedImg = render(uncheckedMenu);

        const QRect row = checkedMenu.actionGeometry(checkedMenu.actions().constFirst());
        bool differs = false;
        for (int y = row.top(); y <= row.bottom(); ++y) {
            for (int x = row.left(); x < row.left() + kItemPadding; ++x) {
                if (checkedImg.pixel(x, y) != uncheckedImg.pixel(x, y))
                    differs = true;
            }
        }
        check(differs, "the checked entry draws a checkmark in the reserved column");
    }

    if (failures > 0) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("All devicemenu checks passed\n");
    return 0;
}
