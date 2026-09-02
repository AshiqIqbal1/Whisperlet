#ifndef TRANSCRIPTCARD_H
#define TRANSCRIPTCARD_H

#include "icons.h"

#include <QDateTime>
#include <QFrame>
#include <QString>

class QLabel;
class QPushButton;
class QToolButton;

// Plain data for one transcription. Swap this for your real model later.
struct Transcript
{
    QString   id;
    QString   text;
    QDateTime when;
    int       durationSec = 0;
};

class TranscriptCard : public QFrame
{
    Q_OBJECT

public:
    explicit TranscriptCard(const Transcript &data, QWidget *parent = nullptr);

    const Transcript &data() const { return m_data; }
    bool matches(const QString &needle) const;

    // Play / re-transcribe only work while the clip's audio is on disk;
    // audio is discarded after transcription unless the user opts to keep it.
    void setAudioAvailable(bool available);

signals:
    void playRequested(const QString &id);
    void copyRequested(const QString &id);
    void retryRequested(const QString &id);
    void deleteRequested(const QString &id);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void toggleExpanded();
    void applyText();
    void setHovered(bool hovered);
    QToolButton *makeAction(Icons::Name icon, const QString &tip, bool danger = false);

    Transcript   m_data;
    bool         m_expanded = false;
    QLabel      *m_text = nullptr;
    QPushButton *m_showMore = nullptr;
    QToolButton *m_play = nullptr;
    QToolButton *m_retry = nullptr;
};

#endif // TRANSCRIPTCARD_H
