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
constexpr int kShadow = 14;
constexpr int kPillHeight = 48;
constexpr int kDot = 12;      // resting diameter of the level dot
constexpr int kLeftInset = 26;

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
    // Left inset leaves room for the painted dot.
    layout->setContentsMargins(kShadow + kLeftInset + kDot + 16, kShadow,
                               kShadow + 32, kShadow);
    layout->setSpacing(0);

    m_text = new QLabel(this);
    m_text->setStyleSheet(QStringLiteral(
        "color:#F5F5F7;font-size:16px;font-weight:600;background:transparent;"));
    layout->addWidget(m_text);

    // Float over every desktop and over fullscreen apps, so the pill is
    // visible wherever the user is actually typing. Done once, before the
    // window is ever shown: see overlaywindow.h for why the timing matters.
    OverlayWindow::configure(this);

    // Drives the dot's response to the voice. Only runs while on screen.
    m_animation = new QTimer(this);
    m_animation->setInterval(50);
    connect(m_animation, &QTimer::timeout, this, [this] {
        // Ease toward the incoming level so the dot swells with the voice
        // instead of twitching at audio chunk rate.
        m_pulse += (m_level - m_pulse) * 0.3;
        if (!m_recording)
            m_phase += 0.18; // gentle breathing while transcribing
        update();
    });
}

void RecordingPill::setLevel(qreal level)
{
    m_level = qBound(0.0, level, 1.0);
}

void RecordingPill::showRecording(const QString &)
{
    m_recording = true;
    m_level = 0.0;
    m_pulse = 0.0;
    m_text->setText(tr("Recording..."));
    m_animation->start();
    showCentered();
}

void RecordingPill::showTranscribing()
{
    m_recording = false;
    m_phase = 0.0;
    m_text->setText(tr("Transcribing..."));
    m_animation->start();
    showCentered();
}

void RecordingPill::showCentered()
{
    adjustSize();

    // On Windows the native window must be rebuilt so it lands on whichever
    // virtual desktop the user is on now. destroy() is protected, so it has
    // to happen here rather than inside the platform helper.
    if (OverlayWindow::recreateBeforeShow() && isHidden()
        && testAttribute(Qt::WA_WState_Created)) {
        destroy();
    }
    OverlayWindow::beforeShow(this);

    // show() only: raise() asks the window server to bring this window (and
    // with it our whole app) forward, which steals focus from the field the
    // user is dictating into. The pill is already always on top.
    show();

    // Position AFTER show: on Windows the native window was just recreated,
    // so anything set before would have been thrown away with it. Doing it
    // here also means the screen is chosen from where the pointer is at the
    // moment the pill appears.
    moveToCurrentScreen();

    OverlayWindow::afterShow(this);
}

void RecordingPill::moveToCurrentScreen()
{
    // Follow the user, not the app: place the pill on whichever screen the
    // pointer is on. Pinning it to the primary screen made it appear on a
    // display the user was not looking at.
    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    const QRect area = screen->availableGeometry();
    int x = area.center().x() - width() / 2;
    int y = area.top() + 10;

    // Clamp inside the target screen: a second display with a different
    // size or scale can otherwise push the pill off its own edge.
    x = qBound(area.left(), x, area.right() - width());
    y = qBound(area.top(), y, area.bottom() - height());
    move(x, y);
}

void RecordingPill::hideEvent(QHideEvent *event)
{
    m_animation->stop(); // no timers while nothing is on screen
    QWidget::hideEvent(event);
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
        p.setBrush(QColor(0, 0, 0, 7));
        p.drawRoundedRect(pill.adjusted(-i, -i + 2, i, i + 2), radius + i, radius + i);
    }

    // Flat, dark capsule. No gradient: it reads as one calm surface.
    p.setBrush(QColor(38, 38, 41, 242));
    p.drawRoundedRect(pill, radius, radius);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 24), 1.0));
    p.drawRoundedRect(pill.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);

    // The one moving part: a dot that swells with your voice while
    // recording, and breathes softly while transcribing.
    const QPointF centre(pill.left() + kLeftInset + kDot / 2.0, pill.center().y());
    const QColor tint = m_recording ? Theme::Danger : Theme::Accent;
    const qreal grow = m_recording ? (0.9 + 0.7 * m_pulse)
                                   : (0.9 + 0.15 * std::sin(m_phase));
    const qreal r = (kDot / 2.0) * grow;

    // A faint halo around it so the movement reads at a glance.
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(tint.red(), tint.green(), tint.blue(), 46));
    p.drawEllipse(centre, r + 5.0, r + 5.0);

    p.setBrush(tint);
    p.drawEllipse(centre, r, r);
}
