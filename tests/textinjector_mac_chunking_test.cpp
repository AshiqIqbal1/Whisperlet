#include "textinjector_mac_chunking.h"

#include <QString>

#include <cstdio>

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
    // Short text: one chunk covers it all.
    {
        const QString text = QStringLiteral("hello");
        const int count = textInjectorMacNextChunkLength(text.utf16(), text.length(), 0, 20);
        check(count == 5, "short text fits in a single chunk");
    }

    // Text longer than the chunk size splits into multiple chunks that
    // together cover the whole string with no gap or overlap.
    {
        const QString text(45, QLatin1Char('a'));
        int i = 0;
        int chunks = 0;
        while (i < text.length()) {
            const int count = textInjectorMacNextChunkLength(text.utf16(), text.length(), i, 20);
            check(count > 0, "each chunk makes forward progress");
            i += count;
            ++chunks;
        }
        check(i == text.length(), "chunks cover the whole string exactly");
        check(chunks == 3, "45 chars at a chunk size of 20 takes 3 chunks");
    }

    // A surrogate pair (e.g. an emoji) landing right on a chunk boundary
    // must not be split: the trailing high surrogate is deferred to the
    // next chunk instead of being carried alone.
    {
        QString text(19, QLatin1Char('a'));
        text += QChar(QChar::highSurrogate(0x1F600));
        text += QChar(QChar::lowSurrogate(0x1F600));
        text += QStringLiteral("bb");

        const int firstCount = textInjectorMacNextChunkLength(text.utf16(), text.length(), 0, 20);
        check(firstCount == 19, "chunk stops before the lone high surrogate at the boundary");

        const int secondCount =
            textInjectorMacNextChunkLength(text.utf16(), text.length(), firstCount, 20);
        check(secondCount == 4, "next chunk carries the full surrogate pair plus following text");
    }

    if (failures > 0) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("All textinjector_mac_chunking checks passed\n");
    return 0;
}
