// Drives the footer "Clear all" (trash) button on the real MainWindow with
// transcripts and kept clips on disk. Regression cover for #77, where a
// search narrowed the list to a few cards and Clear all silently deleted
// every transcript, hidden ones included, with no confirmation.
//
// Checks, against the window, the message box and what is left on disk:
//   - the box names the total and how many cards the search is hiding,
//     with Cancel as the default button;
//   - Cancel leaves every card, the saved history and every clip alone;
//   - Delete All removes exactly the transcripts the box counted. A card
//     that lands while the box is open (a transcription finishing) stays;
//   - the singular wording for one transcript and for one hidden card;
//   - an empty list opens no box at all.
//
// The test runner sets QT_QPA_PLATFORM: offscreen where it can, cocoa on
// macOS because the recording pill's overlay needs a native NSWindow. All
// state lives in a throwaway WhisperletTest/ClearAllConfirmTest data
// directory; the test refuses to run against any other location.

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QTest>
#include <QTimer>
#include <QToolButton>

#include <cstdio>
#include <functional>
#include <vector>

// The only path into the card list from outside the window is a finished
// transcription, which needs a downloaded model and audio. Open the window
// up so the test can call addCard() the way that handler does while the
// confirmation box is sitting open.
#define private public
#define protected public
#include "mainwindow.h"
#undef private
#undef protected

#include "audioclipstore.h"
#include "theme.h"
#include "transcriptstore.h"

namespace {
int failures = 0;

void check(bool condition, const char *what)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

Transcript make(const char *id, const char *text, int day)
{
    return {QString::fromLatin1(id), QString::fromLatin1(text),
            QDateTime(QDate(2026, 10, day), QTime(9, 0)), 5};
}

const QList<Transcript> kSeed{
    make("id-grocery", "Grocery list: eggs, milk, bread and coffee beans.", 1),
    make("id-meeting-one", "Meeting notes: ship the release on Friday after QA signs off.", 2),
    make("id-dentist", "Call the dentist to move the appointment to next Tuesday.", 2),
    make("id-meeting-two", "Meeting follow-up: send the budget spreadsheet to finance.", 3),
};

QStringList savedIds()
{
    QStringList ids;
    const QList<Transcript> saved = TranscriptStore::load();
    for (const Transcript &t : saved)
        ids << t.id;
    return ids;
}

QStringList cardIds(const MainWindow &w)
{
    QStringList ids;
    for (const TranscriptCard *card : w.m_cards)
        ids << card->data().id;
    return ids;
}

QToolButton *clearAllButton(const MainWindow &w)
{
    const auto buttons = w.findChildren<QToolButton *>();
    for (QToolButton *b : buttons) {
        if (b->toolTip() == QStringLiteral("Clear all"))
            return b;
    }
    return nullptr;
}

QPushButton *buttonTitled(const QMessageBox *box, const QString &text)
{
    const auto buttons = box->findChildren<QPushButton *>();
    for (QPushButton *b : buttons) {
        if (b->text().remove(QLatin1Char('&')) == text)
            return b;
    }
    return nullptr;
}

QString statusText(const MainWindow &w)
{
    const auto *label = w.findChild<QLabel *>(QStringLiteral("statusLabel"));
    return label ? label->text() : QString();
}

// Saves a grab of the given widget when WL_EVIDENCE_DIR is set, so a review
// can look at the rendered box rather than only read its strings.
void snapshot(QWidget *widget, const char *name)
{
    const QString dir = qEnvironmentVariable("WL_EVIDENCE_DIR");
    if (dir.isEmpty() || !widget)
        return;
    widget->grab().save(QDir(dir).filePath(QString::fromLatin1(name)));
}

// box.exec() blocks the click handler, so anything that inspects or answers
// the box has to run from the event loop. Polls for the modal box and hands
// it to `fn`; after `timeoutMs` without one, calls `fn(nullptr)`.
void whenBoxOpens(std::function<void(QMessageBox *)> fn, int timeoutMs = 3000)
{
    auto *timer = new QTimer;
    auto *clock = new QElapsedTimer;
    clock->start();
    timer->setInterval(10);
    QObject::connect(timer, &QTimer::timeout, timer, [=] {
        auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        if (!box && clock->elapsed() < timeoutMs)
            return;
        timer->stop();
        timer->deleteLater();
        delete clock;
        fn(box);
    });
    timer->start();
}

void pressClearAll(MainWindow &w)
{
    QToolButton *trash = clearAllButton(w);
    check(trash != nullptr, "footer has a Clear all button");
    if (trash)
        QTest::mouseClick(trash, Qt::LeftButton);
}

void seed()
{
    check(TranscriptStore::save(kSeed), "seed history saved");
    const std::vector<float> samples(16000, 0.1f); // one second of 16kHz
    for (const Transcript &t : kSeed)
        check(AudioClipStore::save(t.id, samples, 16000), "seed clip saved");
}
} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    // Same isolation as the other store tests: test mode plus a throwaway
    // organisation/application pair, so the data directory this test seeds
    // and then empties is never the user's own Whisperlet history.
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("WhisperletTest"));
    QCoreApplication::setApplicationName(QStringLiteral("ClearAllConfirmTest"));
    Theme::pinDarkColorScheme();

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!dataDir.contains(QStringLiteral("WhisperletTest"))
        || !dataDir.contains(QStringLiteral("ClearAllConfirmTest"))) {
        std::fprintf(stderr, "refusing to run: data location %s is not the test scratch directory\n",
                     qPrintable(dataDir));
        return 2;
    }
    QDir(dataDir).removeRecursively(); // leftovers from an interrupted run

    seed();
    MainWindow w;
    w.show();
    check(QTest::qWaitForWindowExposed(&w), "window exposed");
    check(cardIds(w).size() == 4, "window restored the four seeded transcripts");

    auto *search = w.findChild<QLineEdit *>(QStringLiteral("searchBar"));
    check(search != nullptr, "window has the search field");
    if (!search)
        return 1;

    // --- A search hides two of four cards; the box says so; Cancel keeps all.
    QTest::keyClicks(search, QStringLiteral("meeting"));
    {
        bool opened = false;
        whenBoxOpens([&](QMessageBox *box) {
            opened = box != nullptr;
            if (!box)
                return;
            snapshot(box, "harness-box-filtered-two-hidden.png");
            check(box->text() == QStringLiteral("Delete all 4 transcripts?"),
                  "filtered: box counts every transcript, not only the visible ones");
            check(box->informativeText()
                      == QStringLiteral("This includes 2 transcripts hidden by the current search. "
                                        "Kept recordings are deleted too. This can't be undone."),
                  "filtered: box says two transcripts are hidden and that clips go too");
            check(box->icon() == QMessageBox::Warning, "box uses the warning icon");
            QPushButton *deleteAll = buttonTitled(box, QStringLiteral("Delete All"));
            QPushButton *cancel = buttonTitled(box, QStringLiteral("Cancel"));
            check(deleteAll != nullptr, "box offers Delete All");
            check(cancel != nullptr, "box offers Cancel");
            check(deleteAll && box->buttonRole(deleteAll) == QMessageBox::DestructiveRole,
                  "Delete All is the destructive action");
            check(cancel && box->defaultButton() == cancel, "Cancel is the default button");
            if (cancel)
                cancel->click();
        });
        pressClearAll(w);
        check(opened, "filtered: Clear all asks before deleting");
        check(cardIds(w).size() == 4, "cancel: every card is still in the list");
        check(savedIds().size() == 4, "cancel: saved history untouched");
        for (const Transcript &t : kSeed)
            check(AudioClipStore::exists(t.id), "cancel: kept clip still on disk");
        check(statusText(w) != QStringLiteral("Cleared"), "cancel: no 'Cleared' status");
    }

    // --- Escape is the same as Cancel.
    {
        bool opened = false;
        whenBoxOpens([&](QMessageBox *box) {
            opened = box != nullptr;
            if (box)
                QTest::keyClick(box, Qt::Key_Escape);
        });
        pressClearAll(w);
        check(opened, "escape: box opened");
        check(cardIds(w).size() == 4 && savedIds().size() == 4,
              "escape: nothing deleted");
    }

    // --- Delete All with the same search active removes all four, hidden
    // ones included, and their clips. A transcript that lands while the box
    // is open was not counted, so it survives.
    {
        const Transcript late = make("id-late", "Late meeting recap that arrived mid-dialog.", 3);
        whenBoxOpens([&](QMessageBox *box) {
            check(box != nullptr, "delete: box opened");
            if (!box)
                return;
            w.addCard(late, true);
            check(cardIds(w).size() == 5, "delete: late card joined the list while the box was open");
            QPushButton *deleteAll = buttonTitled(box, QStringLiteral("Delete All"));
            check(deleteAll != nullptr, "delete: box offers Delete All");
            if (deleteAll)
                deleteAll->click();
        });
        pressClearAll(w);
        QCoreApplication::processEvents();
        check(cardIds(w) == QStringList{QStringLiteral("id-late")},
              "delete: only the card that arrived during the box remains");
        check(savedIds() == QStringList{QStringLiteral("id-late")},
              "delete: saved history holds only the late transcript");
        for (const Transcript &t : kSeed)
            check(!AudioClipStore::exists(t.id), "delete: counted clip removed from disk");
        check(statusText(w) == QStringLiteral("Cleared"), "delete: status says Cleared");
        // Deleted cards go through deleteLater(); let the loop run so the
        // window shows what the user sees once the handler has returned.
        QTest::qWait(100);
        check(w.findChildren<TranscriptCard *>().size() == 1,
              "delete: the deleted cards' widgets are gone from the window");
        snapshot(&w, "harness-window-after-delete-late-card-kept.png");
    }

    // --- Three cards, a search hiding exactly one: singular hidden wording.
    search->clear();
    w.addCard(make("id-extra-a", "Meeting agenda for Monday.", 3), true);
    w.addCard(make("id-extra-b", "Reminder: water the plants.", 3), true);
    QTest::keyClicks(search, QStringLiteral("meeting"));
    {
        bool opened = false;
        whenBoxOpens([&](QMessageBox *box) {
            opened = box != nullptr;
            if (!box)
                return;
            snapshot(box, "harness-box-one-hidden.png");
            check(box->text() == QStringLiteral("Delete all 3 transcripts?"),
                  "one hidden: box counts all three");
            check(box->informativeText().startsWith(
                      QStringLiteral("This includes 1 transcript hidden by the current search. ")),
                  "one hidden: singular hidden wording");
            if (QPushButton *cancel = buttonTitled(box, QStringLiteral("Cancel")))
                cancel->click();
        });
        pressClearAll(w);
        check(opened, "one hidden: box opened");
        check(cardIds(w).size() == 3, "one hidden: cancel kept all three");
    }

    // --- No search: the box names the total with no mention of hidden cards.
    search->clear();
    {
        bool opened = false;
        whenBoxOpens([&](QMessageBox *box) {
            opened = box != nullptr;
            if (!box)
                return;
            snapshot(box, "harness-box-unfiltered-three.png");
            check(box->text() == QStringLiteral("Delete all 3 transcripts?"),
                  "unfiltered: box counts all three");
            check(box->informativeText()
                      == QStringLiteral("Kept recordings are deleted too. This can't be undone."),
                  "unfiltered: no hidden-count sentence when nothing is hidden");
            if (QPushButton *deleteAll = buttonTitled(box, QStringLiteral("Delete All")))
                deleteAll->click();
        });
        pressClearAll(w);
        QCoreApplication::processEvents();
        check(opened, "unfiltered: box opened");
        check(cardIds(w).isEmpty(), "unfiltered: Delete All emptied the list");
        check(savedIds().isEmpty(), "unfiltered: saved history is empty");
    }

    // --- One transcript left: singular prompt.
    w.addCard(make("id-only", "The only transcript.", 3), true);
    w.persist();
    {
        bool opened = false;
        whenBoxOpens([&](QMessageBox *box) {
            opened = box != nullptr;
            if (!box)
                return;
            snapshot(box, "harness-box-only-one.png");
            check(box->text() == QStringLiteral("Delete your only transcript?"),
                  "single: singular prompt wording");
            if (QPushButton *deleteAll = buttonTitled(box, QStringLiteral("Delete All")))
                deleteAll->click();
        });
        pressClearAll(w);
        QCoreApplication::processEvents();
        check(opened, "single: box opened");
        check(cardIds(w).isEmpty() && savedIds().isEmpty(), "single: Delete All removed it");
    }

    // --- Empty list: pressing the trash opens nothing.
    {
        bool opened = false;
        whenBoxOpens([&](QMessageBox *box) { opened = box != nullptr; }, 300);
        pressClearAll(w);
        QTest::qWait(400);
        check(!opened, "empty: no confirmation box for an empty list");
        check(savedIds().isEmpty(), "empty: history still empty");
    }

    QDir(dataDir).removeRecursively();

    if (failures == 0)
        std::printf("clearall_confirm_test: all checks passed\n");
    return failures == 0 ? 0 : 1;
}
