#include "versioncompare.h"

#include <QVersionNumber>

namespace VersionCompare {

bool isNewer(const QString &latestTag, const QString &currentVersion)
{
    QString cleaned = latestTag;
    if (cleaned.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        cleaned.remove(0, 1);

    const QVersionNumber latest = QVersionNumber::fromString(cleaned);
    const QVersionNumber current = QVersionNumber::fromString(currentVersion);
    if (latest.isNull() || current.isNull())
        return false;

    return QVersionNumber::compare(latest, current) > 0;
}

} // namespace VersionCompare
