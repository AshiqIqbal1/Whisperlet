#include "recordbutton.h"
#include "theme.h"

#include <QEasingCurve>
#include <QFocusEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QRadialGradient>

namespace {
constexpr int kWidgetSize = 148;  // includes room for the halo
constexpr qreal kDiscSize = 78.0; // the visible control
}

RecordButton::RecordButton(QWidget *parent)
    : QAbstractButton(parent)
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_Hover, true);
    setToolTip(tr("Start recording  (Space)"));
    // Icon only, so screen readers need a name; the tooltip is only a hint.
    setAccessibleName(tr("Start recording"));

    m_pulseAnim = new QPropertyAnimation(this, "pulse", this);
    m_pulseAnim->setDuration(1400);
    m_pulseAnim->setStartValue(0.15);
    m_pulseAnim->setEndValue(1.0);
    m_pulseAnim->setEasingCurve(QEasingCurve::InOutSine);
    m_pulseAnim->setLoopCount(-1);

    m_morphAnim = new QPropertyAnimation(this, "morph", this);
    m_morphAnim->setDuration(220);
    m_morphAnim->setEasingCurve(QEasingCurve::OutCubic);
}

QSize RecordButton::sizeHint() const
{
    return {kWidgetSize, kWidgetSize};
}

void RecordButton::setPulse(qreal v)
{
    m_pulse = v;
    update();
}

void RecordButton::setMorph(qreal v)
{
    m_morph = v;
    update();
}

void RecordButton::setLevel(qreal level)
{
    m_level = qBound(0.0, level, 1.0);
    if (m_recording)
        update();
}

void RecordButton::setRecording(bool recording)
{
    if (m_recording == recording)
        return;
    m_recording = recording;

    setToolTip(recording ? tr("Stop recording  (Space)")
                         : tr("Start recording  (Space)"));
    setAccessibleName(recording ? tr("Stop recording") : tr("Start recording"));

    m_morphAnim->stop();
    m_morphAnim->setStartValue(m_morph);
    m_morphAnim->setEndValue(recording ? 1.0 : 0.0);
    m_morphAnim->start();

    if (recording) {
        m_pulseAnim->start();
    } else {
        m_pulseAnim->stop();
        m_pulse = 0.0;
        m_level = 0.0;
    }
    update();
}

void RecordButton::focusInEvent(QFocusEvent *event)
{
    m_keyboardFocus = event->reason() == Qt::TabFocusReason
                      || event->reason() == Qt::BacktabFocusReason;
    QAbstractButton::focusInEvent(event);
    update();
}

void RecordButton::focusOutEvent(QFocusEvent *event)
{
    m_keyboardFocus = false;
    QAbstractButton::focusOutEvent(event);
    update();
}

void RecordButton::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QAbstractButton::enterEvent(event);
}

void RecordButton::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QAbstractButton::leaveEvent(event);
}

void RecordButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QPointF c = rect().center() + QPointF(0.5, 0.5);
    const qreal base = kDiscSize / 2.0;
    const qreal grow = m_hovered ? 2.0 : 0.0;
    const qreal r = base + grow - (isDown() ? 2.0 : 0.0);

    const QColor tint = m_recording ? Theme::Danger : Theme::RecordIdle;

    // --- halo -------------------------------------------------------------
    // Idle: a faint constant bloom. Recording: breathes, and swells with the
    // incoming mic level so you can see the app is hearing you.
    const qreal haloReach = r + 26.0 + (m_recording ? 14.0 * m_pulse + 12.0 * m_level : 0.0);
    QRadialGradient halo(c, haloReach);
    const int coreAlpha = m_recording ? int(70 + 60 * m_pulse) : (m_hovered ? 46 : 30);
    halo.setColorAt(0.0, QColor(tint.red(), tint.green(), tint.blue(), coreAlpha));
    halo.setColorAt(r / haloReach, QColor(tint.red(), tint.green(), tint.blue(), coreAlpha / 2));
    halo.setColorAt(1.0, QColor(tint.red(), tint.green(), tint.blue(), 0));
    p.setPen(Qt::NoPen);
    p.setBrush(halo);
    p.drawEllipse(c, haloReach, haloReach);

    // --- level ring -------------------------------------------------------
    if (m_recording && m_level > 0.01) {
        QPen ring(QColor(Theme::Danger.red(), Theme::Danger.green(), Theme::Danger.blue(), 130));
        ring.setWidthF(2.0 + 5.0 * m_level);
        p.setPen(ring);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(c, r + 10.0 + 6.0 * m_level, r + 10.0 + 6.0 * m_level);
    }

    // --- body -------------------------------------------------------------
    // Interpolate a circle into a rounded square as recording starts.
    const qreal side = r * 2.0 - (r * 0.62) * m_morph;
    const qreal radius = (side / 2.0) * (1.0 - 0.72 * m_morph);
    const QRectF body(c.x() - side / 2.0, c.y() - side / 2.0, side, side);

    p.setPen(Qt::NoPen);
    p.setBrush(tint);
    p.drawRoundedRect(body, radius, radius);

    // Thin outline keeps the disc readable against a light-ish backdrop.
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, m_recording ? 26 : 40), 1.0));
    p.drawRoundedRect(body.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);

    // Only for keyboard navigation: the button keeps focus at launch and
    // after a click (so Space records), and a ring there reads as a stray
    // blue highlight.
    if (hasFocus() && m_keyboardFocus) {
        p.setPen(QPen(Theme::Accent, 2.0));
        p.drawEllipse(c, r + 7.0, r + 7.0);
    }
}
