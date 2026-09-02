#ifndef RECORDINGPILL_H
#define RECORDINGPILL_H

#include <QString>
#include <QWidget>

class QLabel;
class QTimer;

// The floating "● Recording…" pill shown during hotkey dictation — a small
// always-on-top window near the bottom of the screen. It never takes focus
// (critical: the text field the user is dictating into must stay focused).
class RecordingPill : public QWidget
{
    Q_OBJECT

public:
    explicit RecordingPill(QWidget *parent = nullptr);

    // stopHint: label of the key that ends the take, e.g. "Right \u2318".
    void showRecording(const QString &stopHint = QString());
    void showTranscribing();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void showCentered();

    QLabel *m_text = nullptr;
    QTimer *m_pulse = nullptr;
    qreal m_dotAlpha = 1.0;
    bool m_dotGrowing = false;
    bool m_recording = true;
};

#endif // RECORDINGPILL_H
