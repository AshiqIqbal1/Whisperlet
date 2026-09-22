#ifndef DEVICEMENU_H
#define DEVICEMENU_H

#include <QFontMetrics>
#include <QString>

// Widest an input-device name may be drawn in the picker before it is elided.
// Nothing bounds a device name (virtual conferencing inputs run long), and the
// menu sizes itself to its longest item, so without a cap the popup stretches
// past the 560px window it drops out of.
inline constexpr int kDeviceNameMaxWidth = 260;

// The text to show for `name`, shortened with an ellipsis when it does not fit.
// Returns `name` unchanged when it fits, so callers can tell whether they need
// a tooltip carrying the full name.
inline QString deviceMenuLabel(const QFontMetrics &fm, const QString &name,
                               int maxWidth = kDeviceNameMaxWidth)
{
    return fm.elidedText(name, Qt::ElideRight, maxWidth);
}

#endif // DEVICEMENU_H
