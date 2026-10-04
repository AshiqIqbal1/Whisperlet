#ifndef RECORDBUTTON_H
#define RECORDBUTTON_H

#include <QAbstractButton>

class QPropertyAnimation;

// The big circular capture control.
//   idle      -> white disc with a soft glow
//   recording -> red rounded square, breathing halo, live level ring
class RecordButton : public QAbstractButton
{
    Q_OBJECT
    Q_PROPERTY(qreal pulse READ pulse WRITE setPulse)
    Q_PROPERTY(qreal morph READ morph WRITE setMorph)

public:
    explicit RecordButton(QWidget *parent = nullptr);

    QSize sizeHint() const override;

    bool isRecording() const { return m_recording; }
    void setRecording(bool recording);

    // 0.0 - 1.0, drives the ring thickness while recording.
    void setLevel(qreal level);

    qreal pulse() const { return m_pulse; }
    void setPulse(qreal v);
    qreal morph() const { return m_morph; }
    void setMorph(qreal v);

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    bool m_recording = false;
    bool m_hovered = false;
    bool m_keyboardFocus = false; // focus ring only when reached with Tab
    qreal m_pulse = 0.0;   // 0..1 breathing halo
    qreal m_morph = 0.0;   // 0 = disc, 1 = rounded square
    qreal m_level = 0.0;

    QPropertyAnimation *m_pulseAnim = nullptr;
    QPropertyAnimation *m_morphAnim = nullptr;
};

#endif // RECORDBUTTON_H
