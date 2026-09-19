#ifndef TEXTINJECTOR_MAC_CHUNKING_H
#define TEXTINJECTOR_MAC_CHUNKING_H

#include <QChar>

#include <algorithm>

// How many UTF-16 code units of `text` (starting at `start`) the next
// CGEventKeyboardSetUnicodeString chunk should carry, given a target chunk
// size of `maxChunk`. Never returns a count that splits a surrogate pair
// (e.g. an emoji) across two chunks.
inline int textInjectorMacNextChunkLength(const ushort *utf16, int length, int start, int maxChunk)
{
    int count = std::min(maxChunk, length - start);

    if (count < length - start && QChar::isHighSurrogate(utf16[start + count - 1]))
        --count;

    return count;
}

#endif // TEXTINJECTOR_MAC_CHUNKING_H
