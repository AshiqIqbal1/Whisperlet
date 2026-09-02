#ifndef RECORDINGPILL_H
#define RECORDINGPILL_H

#include <QString>
#include <QWidget>

class QLabel;
class QTimer;

// The floating capsule shown while dictating: a live level meter, a label,
// and a hint for the key that stops the take. It is a small always on top
// window near the top of the screen that never takes focus, since the field
// being dictated into has to keep it.
class RecordingPill : public QWidget
{
    Q_OBJECT

public:
    explicit RecordingPill(QWidget *parent = nullptr);

    // stopHint is accepted for call compatibility but deliberately unused:
    // the pill stays a single calm line, not an instruction panel.
    void showRecording(const QString &stopHint = QString());
    void showTranscribing();

    // 0..1 microphone level, drives the meter while recording.
    void setLevel(qreal level);

protected:
    void paintEvent(QPaintEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void showCentered();

    QLabel *m_text = nullptr;
    QTimer *m_animation = nullptr;

    qreal m_level = 0.0;  // incoming mic level, 0..1
    qreal m_pulse = 0.0;  // smoothed level actually drawn
    qreal m_phase = 0.0;  // breathing phase while transcribing
    bool m_recording = true;
};

#endif // RECORDINGPILL_H
