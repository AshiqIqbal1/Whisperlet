// Icon-only buttons must have an accessible name (issue #75). A tooltip is
// exposed only as the accessible description, so without a name VoiceOver
// and Narrator announce a bare "button". Reads each control's name through
// QAccessible, the same interface screen readers use. MainWindow itself
// can't be built under the offscreen platform (see playback_trigger_test),
// so this covers the shared pieces: transcript card actions, the title bar
// and the record button.

#include "recordbutton.h"
#include "titlebar.h"
#include "transcriptcard.h"

#include <QAccessible>
#include <QApplication>
#include <QToolButton>

#include <cstdio>

namespace {
int failures = 0;

void check(bool condition, const char *what)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

QString accessibleName(QWidget *widget)
{
    QAccessibleInterface *iface = QAccessible::queryAccessibleInterface(widget);
    return iface ? iface->text(QAccessible::Name) : QString();
}

// Every icon-only tool button under root is named after what it does.
void checkToolButtonsNamed(QWidget *root, const char *where)
{
    const auto buttons = root->findChildren<QToolButton *>();
    check(!buttons.isEmpty(), where);
    for (QToolButton *button : buttons) {
        const QString name = accessibleName(button);
        if (name.isEmpty() || name != button->toolTip()) {
            std::fprintf(stderr, "FAIL: %s: button with tooltip '%s' has accessible name '%s'\n", where,
                         qPrintable(button->toolTip()), qPrintable(name));
            ++failures;
        }
    }
}
} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    Transcript t{QStringLiteral("a11y-test"), QStringLiteral("hello"), QDateTime::currentDateTime(), 2};
    TranscriptCard card(t);
    card.setAudioAvailable(true);
    checkToolButtonsNamed(&card, "transcript card actions are named");

    TitleBar titleBar;
    checkToolButtonsNamed(&titleBar, "title bar buttons are named");

    RecordButton record;
    check(accessibleName(&record) == QStringLiteral("Start recording"), "record button is named when idle");
    record.setRecording(true);
    check(accessibleName(&record) == QStringLiteral("Stop recording"), "record button name follows recording state");
    record.setRecording(false);
    check(accessibleName(&record) == QStringLiteral("Start recording"), "record button name returns to idle");

    if (failures == 0)
        std::printf("All accessible-name tests passed.\n");
    return failures == 0 ? 0 : 1;
}
