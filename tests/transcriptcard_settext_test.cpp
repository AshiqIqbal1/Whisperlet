// "Transcribe again" updates a card's text in place (issue #76). Checks that
// TranscriptCard::setText() replaces the stored and shown text, and keeps
// the card's expanded Show more/less state instead of resetting it.

#include "transcriptcard.h"

#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QTest>

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

bool anyLabelShows(const TranscriptCard &card, const QString &text)
{
    const auto labels = card.findChildren<QLabel *>();
    for (const QLabel *label : labels) {
        if (label->text() == text)
            return true;
    }
    return false;
}
} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    const QString longText = QStringLiteral("word ").repeated(200).trimmed();
    Transcript t{QStringLiteral("settext-test"), longText, QDateTime::currentDateTime(), 3};
    TranscriptCard card(t);
    card.show();

    QPushButton *showMore = nullptr;
    for (auto *button : card.findChildren<QPushButton *>()) {
        if (button->text() == QStringLiteral("Show more"))
            showMore = button;
    }
    check(showMore != nullptr, "a long transcript has a Show more button");
    if (showMore)
        QTest::mouseClick(showMore, Qt::LeftButton);
    check(showMore && showMore->text() == QStringLiteral("Show less"), "the card is expanded");

    const QString updated = QStringLiteral("again ").repeated(200).trimmed();
    card.setText(updated);
    check(card.data().text == updated, "setText replaces the stored transcript text");
    check(card.data().id == t.id && card.data().when == t.when, "setText keeps the id and date");
    check(anyLabelShows(card, updated), "the expanded card shows the full new text");
    check(showMore && showMore->text() == QStringLiteral("Show less"), "the card stays expanded");

    card.setText(QStringLiteral("short"));
    check(anyLabelShows(card, QStringLiteral("short")), "a short new text is shown as is");
    check(showMore && showMore->isHidden(), "Show more hides when the new text needs no truncation");

    if (failures == 0)
        std::printf("All transcript-card setText tests passed.\n");
    return failures == 0 ? 0 : 1;
}
