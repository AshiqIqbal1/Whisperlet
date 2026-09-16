#ifndef VERSIONCOMPARE_H
#define VERSIONCOMPARE_H

#include <QString>

// Pure version-string comparison, split out from UpdateChecker so it can be
// unit tested without a network round trip or a Qt event loop.
namespace VersionCompare {

// True if `latestTag` (e.g. "v0.8.0", "0.8.0") names a version newer than
// `currentVersion` (e.g. "0.7.1"). A malformed tag or version never counts
// as newer — silence beats a false "update available" from garbage input.
bool isNewer(const QString &latestTag, const QString &currentVersion);

} // namespace VersionCompare

#endif // VERSIONCOMPARE_H
