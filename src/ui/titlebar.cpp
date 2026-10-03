#include "titlebar.h"

#include "icons.h"
#include "theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QToolButton>
#include <QWindow>

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(44);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 0, 10, 0);
    layout->setSpacing(8);

    auto *logo = new QLabel(this);
    logo->setPixmap(QPixmap(QStringLiteral(":/assets/icon-64.png"))
                        .scaled(22, 22, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    layout->addWidget(logo);

    auto *name = new QLabel(QStringLiteral("Whisperlet"), this);
    name->setStyleSheet(QStringLiteral("color:#8A8A93;font-size:13px;font-weight:600;"));
    layout->addWidget(name);

    layout->addStretch(1);

    auto *minBtn = new QToolButton(this);
    minBtn->setIcon(Icons::icon(Icons::Minimize, Theme::TextMuted, 14));
    minBtn->setFixedSize(30, 30);
    minBtn->setToolTip(tr("Minimize"));
    minBtn->setAccessibleName(minBtn->toolTip());
    connect(minBtn, &QToolButton::clicked, this, [this] { window()->showMinimized(); });
    layout->addWidget(minBtn);

    auto *closeBtn = new QToolButton(this);
    closeBtn->setIcon(Icons::icon(Icons::Close, Theme::TextMuted, 14));
    closeBtn->setFixedSize(30, 30);
    closeBtn->setToolTip(tr("Close"));
    closeBtn->setAccessibleName(closeBtn->toolTip());
    connect(closeBtn, &QToolButton::clicked, this, [this] { window()->close(); });
    layout->addWidget(closeBtn);
}

void TitleBar::mousePressEvent(QMouseEvent *event)
{
    // Hand the drag to the OS: native move keeps things like screen-edge
    // behavior working even though the window is frameless.
    if (event->button() == Qt::LeftButton && window()->windowHandle()) {
        window()->windowHandle()->startSystemMove();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}
