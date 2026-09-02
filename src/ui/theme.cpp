#include "theme.h"

QString Theme::styleSheet()
{
    return QStringLiteral(R"(
QWidget {
    background: transparent;
    color: #F2F2F5;
    font-size: 14px;
}

QMainWindow {
    background: transparent;
}
#root {
    background: #141416;
    border-top-left-radius: 12px;
    border-top-right-radius: 12px;
}

/* ---- search ---- */
#searchBar {
    background: #1E1E21;
    border: 1px solid #303036;
    border-radius: 12px;
    padding: 9px 12px;
    font-size: 15px;
    selection-background-color: #0A84FF;
}
#searchBar:focus {
    border: 1px solid #0A84FF;
    background: #232327;
}

/* ---- transcript card ---- */
#card {
    background: #1E1E21;
    border: 1px solid #2A2A30;
    border-radius: 14px;
}
#card:focus { border: 1px solid #3A3A42; }
#card[hovered="true"] {
    background: #232327;
    border: 1px solid #3A3A42;
}

#cardText {
    font-size: 15px;
    line-height: 22px;
    color: #E8E8EC;
}
#cardMeta {
    font-size: 12px;
    color: #8A8A93;
}
#durationChip {
    background: #2A2A30;
    border-radius: 8px;
    padding: 2px 8px;
    font-size: 11px;
    color: #A0A0A8;
}
#separator {
    background: #2A2A30;
    max-height: 1px;
    border: none;
}

#showMore {
    background: transparent;
    border: none;
    color: #0A84FF;
    font-size: 13px;
    padding: 2px 0;
    text-align: left;
}
#showMore:hover { color: #4DA6FF; }

/* ---- icon buttons ---- */
QToolButton {
    background: transparent;
    border: none;
    border-radius: 8px;
    padding: 5px;
}
QToolButton:hover   { background: #303038; }
QToolButton:pressed { background: #3A3A44; }

QToolButton#playBtn {
    background: #0A84FF;
    border-radius: 14px;
}
QToolButton#playBtn:hover   { background: #3D9DFF; }
QToolButton#playBtn:pressed { background: #0768CC; }

QToolButton#dangerBtn:hover { background: #4A2320; }

QToolButton#footerBtn {
    background: #1E1E21;
    border: 1px solid #303036;
    border-radius: 10px;
    padding: 8px;
}
QToolButton#footerBtn:hover { background: #292930; }

/* ---- footer hints ---- */
#hint {
    color: #6E6E77;
    font-size: 12px;
    padding-left: 2px;
}
#kbd {
    background: #232327;
    border: 1px solid #34343C;
    border-radius: 5px;
    color: #9A9AA3;
    font-size: 11px;
    padding: 1px 6px;
}

#statusLabel {
    color: #8A8A93;
    font-size: 13px;
    padding: 2px 0;
}

#emptyTitle {
    color: #6E6E77;
    font-size: 15px;
    line-height: 24px;
}

/* ---- scroll area ---- */
QScrollArea, #scrollBody { background: transparent; border: none; }

QScrollBar:vertical {
    background: transparent;
    width: 0px;
    margin: 0px;
}
QScrollBar::handle:vertical { background: transparent; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }

QToolTip {
    background: #26262A;
    color: #E8E8EC;
    border: 1px solid #3A3A42;
    border-radius: 6px;
    padding: 4px 7px;
}
)");
}
