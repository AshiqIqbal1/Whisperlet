#include "recordingpill.h"

#include "overlaywindow.h"
#include "theme.h"

#include <QCursor>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QScreen>
#include <QTimer>

#include <cmath>

namespace {

// The window is larger than the capsule so a soft shadow can be painted
// around it; nothing else is drawn in that margin.
constexpr int kShadow = 16;
constexpr int kPillHeight = 46;
constexpr int kMeterWidth = 26;

} // namespace

RecordingPill::RecordingPill(QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint
                          | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus)
{
    // Never steal focus: the field being dictated into must keep it.
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    // Qt hides tool windows on macOS as soon as the application is
    // deactivated, which is precisely when this pill needs to be visible:
    // the user is dictating into someone else's window.
    setAttribute(Qt::WA_MacAlwaysShowToolWindow, true);
    setFixedHeight(kPillHeight + kShadow * 2);

    auto *layout = new QHBoxLayout(this);
    // Left inset leaves room for the painted meter.
    layout->setContentsMargins(kShadow + 20 + kMeterWidth + 14, kShadow,
                               kShadow + 22, kShadow);
    layout->setSpacing(10);

    m_text = new QLabel(this);
    m_text->setStyleSheet(QStringLiteral(
        "color:#F5F5F7;font-size:14px;font-weight:600;background:transparent;"));
    layout->addWidget(m_text);

    m_hint = new QLabel(this);
    m_hint->setStyleSheet(QStringLiteral(
        "color:#8A8A93;font-size:12px;background:transparent;"));
    layout->addWidget(m_hint);

    // Float over every Space and over fullscreen apps, so the pill is
    // visible wherever the user is actually typing.
    OverlayWindow::makeFloatingOverlay(this);

    // One timer drives both the meter and the transcribing spinner. 20fps is
    // smooth enough here and only runs while the pill is on screen.
    m_animation = new QTimer(this);
    m_animation->setInterval(50);
    connect(m_animation, &QTimer::timeout, this, [this] {
        if (m_recording) {
            // Ease each bar toward its target so the meter breathes instead
            // of flickering, with a little variation per bar.
            for (int i = 0; i < kBars; ++i) {
                const qreal weight = (i == kBars / 2) ? 1.0 : (i % 2 ? 0.62 : 0.82);
                m_targets[i] = 0.12 + m_level * weight
                               * (0.75 + 0.25 * std::sin(m_phase * 2.0 + i));
                m_bars[i] += (m_targets[i] - m_bars[i]) * 0.35;
            }
            m_phase += 0.25;
        } else {
            m_phase += 0.16;
        }
        update();
    });
}

void RecordingPill::setLevel(qreal level)
{
    m_level = qBound(0.0, level, 1.0);
}

void RecordingPill::showRecording(const QString &stopHint)
{
    m_recording = true;
    m_level = 0.0;
    m_bars.fill(0.12);
    m_text->setText(tr("Recording"));
    m_hint->setText(stopHint.isEmpty() ? QString() : tr("%1 to stop").arg(stopHint));
    m_hint->setVisible(!stopHint.isEmpty());
    m_animation->start();
    showCentered();
}

void RecordingPill::showTranscribing()
{
    m_recording = false;
    m_text->setText(tr("Transcribing"));
    m_hint->setVisible(false);
    m_animation->start();
    showCentered();
}

void RecordingPill::showCentered()
{
    adjustSize();

    // Follow the user, not the app: place the pill on whichever screen the
    // pointer is on. Pinning it to the primary screen made it appear on a
    // display the user wasn't looking at.
    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (screen) {
        const QRect area = screen->availableGeometry();
        // Near the top, under the menu bar, where a status readout is
        // expected and little of value is usually covered.
        move(area.center().x() - width() / 2, area.top() + 10);
    }

    // show() only: raise() asks the window server to bring this window (and
    // with it our whole app) forward, which steals focus from the field the
    // user is dictating into. The pill is already always-on-top.
    show();

    // Re-apply after show(): Qt can recreate the native window between
    // hide and show, which drops the all-Spaces collection behavior.
    OverlayWindow::makeFloatingOverlay(this);
}

void RecordingPill::hideEvent(QHideEvent *event)
{
    m_animation->stop(); // no timers while nothing is on screen
    QWidget::hideEvent(event);
}

void RecordingPill::drawMeter(QPainter &p, const QRectF &area)
{
    const qreal barW = 3.0;
    const qreal gap = (area.width() - kBars * barW) / (kBars - 1);
    const qreal maxH = area.height();

    p.setPen(Qt::NoPen);
    p.setBrush(Theme::Danger);

    qreal x = area.left();
    for (int i = 0; i < kBars; ++i) {
        const qreal h = qBound(0.12, m_bars[i], 1.0) * maxH;
        p.drawRoundedRect(QRectF(x, area.center().y() - h / 2.0, barW, h),
                          barW / 2.0, barW / 2.0);
        x += barW + gap;
    }
}

void RecordingPill::drawSpinner(QPainter &p, const QRectF &area)
{
    const QPointF c = area.center();
    const qreal r = std::min(area.width(), area.height()) / 2.0 - 1.0;

    QPen pen(QColor(Theme::Accent.red(), Theme::Accent.green(), Theme::Accent.blue(), 80));
    pen.setWidthF(2.5);
    pen.setCapStyle(Qt::RoundCap);
    p.setBrush(Qt::NoBrush);
    p.setPen(pen);
    p.drawEllipse(c, r, r);

    pen.setColor(Theme::Accent);
    p.setPen(pen);
    const int startAngle = int(-m_phase * 300.0) * 16;
    p.drawArc(QRectF(c.x() - r, c.y() - r, r * 2, r * 2), startAngle, 100 * 16);
}

void RecordingPill::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF pill = QRectF(rect()).adjusted(kShadow, kShadow, -kShadow, -kShadow);
    const qreal radius = pill.height() / 2.0;

    // Soft shadow: concentric rounded rects at low alpha. Cheaper than a
    // blur effect, and it only repaints while the pill is on screen.
    p.setPen(Qt::NoPen);
    for (int i = kShadow; i > 0; i -= 3) {
        p.setBrush(QColor(0, 0, 0, 6 + (kShadow - i) / 2));
        p.drawRoundedRect(pill.adjusted(-i, -i + 2, i, i + 2), radius + i, radius + i);
    }

    // Capsule body, slightly lighter at the top so it reads as a surface
    // rather than a flat blob.
    QLinearGradient body(pill.topLeft(), pill.bottomLeft());
    body.setColorAt(0.0, QColor(46, 46, 50, 246));
    body.setColorAt(1.0, QColor(28, 28, 31, 246));
    p.setBrush(body);
    p.drawRoundedRect(pill, radius, radius);

    // Hairline edge for definition against light wallpapers.
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 30), 1.0));
    p.drawRoundedRect(pill.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);

    const QRectF indicator(pill.left() + 20, pill.center().y() - 9, kMeterWidth, 18);
    if (m_recording)
        drawMeter(p, indicator);
    else
        drawSpinner(p, indicator);
}
