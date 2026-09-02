#include "transcriptstore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace {

QString filePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dir);
    return QDir(dir).filePath(QStringLiteral("transcripts.json"));
}

} // namespace

QList<Transcript> TranscriptStore::load()
{
    QList<Transcript> result;

    QFile file(filePath());
    if (!file.open(QIODevice::ReadOnly))
        return result; // first run — no file yet, that's fine

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray())
        return result;

    for (const QJsonValue &v : doc.array()) {
        const QJsonObject o = v.toObject();
        Transcript t;
        t.id = o.value(QStringLiteral("id")).toString();
        t.text = o.value(QStringLiteral("text")).toString();
        t.when = QDateTime::fromString(o.value(QStringLiteral("when")).toString(), Qt::ISODate);
        t.durationSec = o.value(QStringLiteral("durationSec")).toInt();
        if (!t.id.isEmpty())
            result.append(t);
    }
    return result;
}

void TranscriptStore::save(const QList<Transcript> &transcripts)
{
    QJsonArray arr;
    for (const Transcript &t : transcripts) {
        QJsonObject o;
        o[QStringLiteral("id")] = t.id;
        o[QStringLiteral("text")] = t.text;
        o[QStringLiteral("when")] = t.when.toString(Qt::ISODate);
        o[QStringLiteral("durationSec")] = t.durationSec;
        arr.append(o);
    }

    QFile file(filePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    // Transcripts are whatever the user dictated: notes, messages, possibly
    // credentials read aloud. Default file mode is world readable, so on a
    // shared machine any other local account could read them.
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}
