#include "recordingpill.h"

#include "overlaywindow.h"
#include "theme.h"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QScreen>
#include <QTimer>

namespace {
constexpr int kDotSize = 10;
}

RecordingPill::RecordingPill(QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint
                          | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus)
{
    // Never steal focus — the field being dictated into must keep it.
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    // Qt hides tool windows on macOS as soon as the application is
    // deactivated, which is precisely when this pill needs to be visible:
    // the user is dictating into someone else's window.
    setAttribute(Qt::WA_MacAlwaysShowToolWindow, true);
    setFixedHeight(44);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(38, 0, 22, 0); // left room for the painted dot

    m_text = new QLabel(this);
    m_text->setStyleSheet(QStringLiteral("color:#F2F2F5;font-size:14px;font-weight:600;background:transparent;"));
    layout->addWidget(m_text);

    // Low-rate blink (2 Hz) instead of a continuous fade: visually the same
    // "alive" cue at a fraction of the wakeups/repaints — battery matters.
    // Float over every Space and over fullscreen apps, so the pill is
    // visible wherever the user is actually typing.
    OverlayWindow::makeFloatingOverlay(this);

    m_pulse = new QTimer(this);
    m_pulse->setInterval(500);
    connect(m_pulse, &QTimer::timeout, this, [this] {
        m_dotAlpha = qFuzzyCompare(m_dotAlpha, 1.0) ? 0.35 : 1.0;
        update();
    });
}

void RecordingPill::showRecording(const QString &stopHint)
{
    m_recording = true;
    m_text->setText(stopHint.isEmpty()
                        ? tr("Recording…")
                        : tr("Recording…   %1 to stop").arg(stopHint));
    m_dotAlpha = 1.0;
    m_pulse->start();
    showCentered();
}

void RecordingPill::showTranscribing()
{
    m_recording = false;
    m_text->setText(tr("Transcribing…"));
    m_pulse->stop(); // steady dot — no timer while nothing blinks
    m_dotAlpha = 1.0;
    showCentered();
}

void RecordingPill::showCentered()
{
    adjustSize();
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect area = screen->availableGeometry();
        move(area.center().x() - width() / 2, area.bottom() - 110);
    }
    // show() only: raise() asks the window server to bring this window (and
    // with it our whole app) forward, which steals focus from the field the
    // user is dictating into. The pill is already always-on-top.
    show();

    // Re-apply after show(): Qt can recreate the native window between
    // hide and show, which drops the all-Spaces collection behavior.
    OverlayWindow::makeFloatingOverlay(this);
}

void RecordingPill::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Pill body
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(30, 30, 33, 242));
    p.drawRoundedRect(rect(), height() / 2.0, height() / 2.0);

    // Status dot: red while recording, accent-blue while transcribing
    QColor dot = m_recording ? Theme::Danger : Theme::Accent;
    dot.setAlphaF(m_dotAlpha);
    p.setBrush(dot);
    p.drawEllipse(QPointF(22, height() / 2.0), kDotSize / 2.0, kDotSize / 2.0);
}
