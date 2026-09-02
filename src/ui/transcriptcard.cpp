#include "transcriptcard.h"

#include "theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kCollapsedChars = 190;

QString durationText(int seconds)
{
    return QStringLiteral("%1:%2")
        .arg(seconds / 60)
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

} // namespace

TranscriptCard::TranscriptCard(const Transcript &data, QWidget *parent)
    : QFrame(parent)
    , m_data(data)
{
    setObjectName(QStringLiteral("card"));
    setProperty("hovered", false);
    setAttribute(Qt::WA_StyledBackground, true);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 12);
    root->setSpacing(10);

    // --- body text --------------------------------------------------------
    m_text = new QLabel(this);
    m_text->setObjectName(QStringLiteral("cardText"));
    m_text->setWordWrap(true);
    m_text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_text);

    m_showMore = new QPushButton(this);
    m_showMore->setObjectName(QStringLiteral("showMore"));
    m_showMore->setCursor(Qt::PointingHandCursor);
    m_showMore->setFlat(true);
    m_showMore->setIconSize({14, 14});
    connect(m_showMore, &QPushButton::clicked, this, &TranscriptCard::toggleExpanded);

    auto *showMoreRow = new QHBoxLayout;
    showMoreRow->setContentsMargins(0, 0, 0, 0);
    showMoreRow->addWidget(m_showMore);
    showMoreRow->addStretch(1);
    root->addLayout(showMoreRow);

    // --- divider ----------------------------------------------------------
    auto *line = new QFrame(this);
    line->setObjectName(QStringLiteral("separator"));
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);
    root->addWidget(line);

    // --- footer: timestamp on the left, actions on the right --------------
    auto *footer = new QHBoxLayout;
    footer->setContentsMargins(0, 0, 0, 0);
    footer->setSpacing(8);

    auto *meta = new QLabel(this);
    meta->setObjectName(QStringLiteral("cardMeta"));
    meta->setText(m_data.when.toString(QStringLiteral("d MMMM yyyy  ·  h:mm ap")));
    footer->addWidget(meta);

    if (m_data.durationSec > 0) {
        auto *chip = new QLabel(durationText(m_data.durationSec), this);
        chip->setObjectName(QStringLiteral("durationChip"));
        footer->addWidget(chip);
    }

    footer->addStretch(1);

    m_play = makeAction(Icons::Play, tr("Play audio"));
    m_play->setObjectName(QStringLiteral("playBtn"));
    m_play->setIcon(Icons::icon(Icons::Play, Qt::white, 16));
    m_play->setFixedSize(28, 28);
    connect(m_play, &QToolButton::clicked, this, [this] { emit playRequested(m_data.id); });
    footer->addWidget(m_play);

    auto *copy = makeAction(Icons::Copy, tr("Copy text"));
    connect(copy, &QToolButton::clicked, this, [this] { emit copyRequested(m_data.id); });
    footer->addWidget(copy);

    m_retry = makeAction(Icons::Retry, tr("Transcribe again"));
    connect(m_retry, &QToolButton::clicked, this, [this] { emit retryRequested(m_data.id); });
    footer->addWidget(m_retry);

    auto *del = makeAction(Icons::Trash, tr("Delete"), /*danger=*/true);
    connect(del, &QToolButton::clicked, this, [this] { emit deleteRequested(m_data.id); });
    footer->addWidget(del);

    root->addLayout(footer);

    applyText();
}

QToolButton *TranscriptCard::makeAction(Icons::Name icon, const QString &tip, bool danger)
{
    auto *b = new QToolButton(this);
    b->setIcon(Icons::icon(icon, danger ? Theme::Danger : Theme::TextMuted, 17));
    b->setIconSize({17, 17});
    b->setToolTip(tip);
    b->setCursor(Qt::PointingHandCursor);
    b->setFixedSize(28, 28);
    if (danger)
        b->setObjectName(QStringLiteral("dangerBtn"));
    return b;
}

void TranscriptCard::applyText()
{
    const bool needsTruncation = m_data.text.size() > kCollapsedChars;

    if (!needsTruncation || m_expanded) {
        m_text->setText(m_data.text);
    } else {
        // Cut on a word boundary so the preview never ends mid-word.
        int cut = m_data.text.lastIndexOf(QLatin1Char(' '), kCollapsedChars);
        if (cut < kCollapsedChars / 2)
            cut = kCollapsedChars;
        m_text->setText(m_data.text.left(cut).trimmed() + QStringLiteral("…"));
    }

    m_showMore->setVisible(needsTruncation);
    m_showMore->setText(m_expanded ? tr("Show less") : tr("Show more"));
    m_showMore->setIcon(Icons::icon(m_expanded ? Icons::ChevronUp : Icons::ChevronDown,
                                    Theme::Accent, 14));
}

void TranscriptCard::toggleExpanded()
{
    m_expanded = !m_expanded;
    applyText();
}

void TranscriptCard::setAudioAvailable(bool available)
{
    // Hidden rather than disabled: a greyed-out button on every card would
    // just be noise once audio is discarded by default.
    if (m_play)
        m_play->setVisible(available);
    if (m_retry)
        m_retry->setVisible(available);
}

bool TranscriptCard::matches(const QString &needle) const
{
    return needle.isEmpty() || m_data.text.contains(needle, Qt::CaseInsensitive);
}

void TranscriptCard::setHovered(bool hovered)
{
    // Dynamic properties drive the QSS selector, so re-polish to repaint.
    setProperty("hovered", hovered);
    style()->unpolish(this);
    style()->polish(this);
}

void TranscriptCard::enterEvent(QEnterEvent *event)
{
    setHovered(true);
    QFrame::enterEvent(event);
}

void TranscriptCard::leaveEvent(QEvent *event)
{
    setHovered(false);
    QFrame::leaveEvent(event);
}
