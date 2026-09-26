#ifndef THEME_H
#define THEME_H

#include <QColor>
#include <QString>

// Single source of truth for colours + the global stylesheet.
// Anything that paints by hand (RecordButton) reads the constants;
// anything that is a plain widget is styled by the QSS below.
namespace Theme {

inline const QColor Window      {0x14, 0x14, 0x16};
inline const QColor Surface     {0x1E, 0x1E, 0x21};
inline const QColor SurfaceHi   {0x26, 0x26, 0x2A};
inline const QColor Border      {0x30, 0x30, 0x36};
inline const QColor TextPrimary {0xF2, 0xF2, 0xF5};
inline const QColor TextMuted   {0x8A, 0x8A, 0x93};
inline const QColor TextFaint   {0x5E, 0x5E, 0x66};
inline const QColor Accent      {0x0A, 0x84, 0xFF};
inline const QColor Danger      {0xFF, 0x45, 0x3A};
inline const QColor RecordIdle  {0xF5, 0xF5, 0xF7};

QString styleSheet();

// The stylesheet only covers the dark look. Widgets it leaves to the native
// style (radio button and check box indicators) otherwise follow the system
// appearance and nearly vanish on the dark background in Light mode, so pin
// the app to the dark colour scheme. Call once after QApplication exists.
// No-op before Qt 6.8, which has no API for it.
void pinDarkColorScheme();

} // namespace Theme

#endif // THEME_H
