#ifndef TRANSCRIPTSTORE_H
#define TRANSCRIPTSTORE_H

#include "transcriptcard.h"

#include <QList>

// Flat JSON file on disk: <AppLocalDataLocation>/transcripts.json
// Deliberately not a database — this is a personal transcript list, not
// data at a scale that needs one. Swap for SQLite later if it grows.
namespace TranscriptStore {

// A file that exists but doesn't parse is moved aside to
// "transcripts.json.corrupt-<timestamp>" (and logged) rather than being
// treated as empty history and overwritten by the next save().
QList<Transcript> load();

// Atomic: the file is only replaced once every byte is written. Returns
// false, leaving the previous file untouched, if anything failed.
bool save(const QList<Transcript> &transcripts);

} // namespace TranscriptStore

#endif // TRANSCRIPTSTORE_H
