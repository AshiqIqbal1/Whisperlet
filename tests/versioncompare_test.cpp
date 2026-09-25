#include "versioncompare.h"

#include <cstdio>
#include <cstdlib>

namespace {
int failures = 0;

void check(bool condition, const char *what)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}
} // namespace

int main()
{
    using VersionCompare::isNewer;

    check(isNewer("v0.8.0", "0.7.1"), "v-prefixed newer tag beats current");
    check(isNewer("0.8.0", "0.7.1"), "newer tag beats current");
    check(isNewer("1.0.0", "0.7.1"), "major bump is newer");
    check(isNewer("0.7.2", "0.7.1"), "patch bump is newer");

    check(!isNewer("0.7.1", "0.7.1"), "identical versions are not newer");
    check(!isNewer("v0.7.1", "0.7.1"), "identical versions with v prefix are not newer");
    check(!isNewer("0.7.0", "0.7.1"), "older tag is not newer");

    // Weekly releases are tagged vYYYY.MM.DD and CI passes the tag minus
    // its "v" as the app version, so a build of that release reports e.g.
    // "2026.09.28" (leading zeros included) and must match its own tag.
    check(!isNewer("v2026.09.28", "2026.09.28"), "a date-tagged build is not behind its own tag");
    check(isNewer("v2026.10.05", "2026.09.28"), "a later weekly release is newer");
    check(!isNewer("v2026.09.21", "2026.09.28"), "an earlier weekly release is not newer");
    check(isNewer("v2026.09.28", "0.7.1"), "a weekly release is newer than a semver build");

    check(!isNewer("", "0.7.1"), "empty tag never counts as newer");
    check(!isNewer("not-a-version", "0.7.1"), "garbage tag never counts as newer");
    check(!isNewer("v0.8.0", ""), "empty current version never triggers an update");
    check(!isNewer("v0.8.0", "not-a-version"), "garbage current version never triggers an update");

    if (failures == 0)
        std::printf("All version comparison tests passed.\n");

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
