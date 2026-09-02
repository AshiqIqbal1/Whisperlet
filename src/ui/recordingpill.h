#ifndef RECORDINGPILL_H
#define RECORDINGPILL_H

#include <QString>
#include <QWidget>

#include <array>

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

    // stopHint: label of the key that ends the take, e.g. "Right Cmd".
    void showRecording(const QString &stopHint = QString());
    void showTranscribing();

    // 0..1 microphone level, drives the meter while recording.
    void setLevel(qreal level);

protected:
    void paintEvent(QPaintEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void showCentered();
    void drawMeter(QPainter &p, const QRectF &area);
    void drawSpinner(QPainter &p, const QRectF &area);

    QLabel *m_text = nullptr;
    QLabel *m_hint = nullptr;
    QTimer *m_animation = nullptr;

    static constexpr int kBars = 5;
    std::array<qreal, kBars> m_bars{};   // current bar heights, 0..1
    std::array<qreal, kBars> m_targets{}; // where each bar is heading
    qreal m_level = 0.0;
    qreal m_phase = 0.0;   // transcribing spinner rotation
    bool m_recording = true;
};

#endif // RECORDINGPILL_H
