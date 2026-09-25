#include "transcriptstore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QtDebug>

namespace {

QString filePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dir);
    return QDir(dir).filePath(QStringLiteral("transcripts.json"));
}

// A history file that exists but doesn't parse was cut short or damaged
// (killed mid-write by an older build, disk trouble). Treating it as "no
// history" would let the next save() overwrite it for good, so move it
// aside where it can still be recovered by hand.
void moveAside(QFile &file, const QString &reason)
{
    file.close();
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    QString target = file.fileName() + QStringLiteral(".corrupt-") + stamp;
    for (int n = 1; QFile::exists(target); ++n)
        target = file.fileName() + QStringLiteral(".corrupt-") + stamp + QLatin1Char('-') + QString::number(n);

    if (QFile::rename(file.fileName(), target))
        qWarning("TranscriptStore: %s is damaged (%s), moved it to %s and starting with empty history",
                 qPrintable(file.fileName()), qPrintable(reason), qPrintable(target));
    else
        qWarning("TranscriptStore: %s is damaged (%s) and could not be moved aside: %s",
                 qPrintable(file.fileName()), qPrintable(reason), qPrintable(file.errorString()));
}

} // namespace

QList<Transcript> TranscriptStore::load()
{
    QList<Transcript> result;

    QFile file(filePath());
    if (!file.exists())
        return result; // first run - no file yet, that's fine
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("TranscriptStore: cannot read %s: %s",
                 qPrintable(file.fileName()), qPrintable(file.errorString()));
        return result;
    }

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        moveAside(file, error.errorString());
        return result;
    }
    if (!doc.isArray()) {
        moveAside(file, QStringLiteral("not a JSON array"));
        return result;
    }

    for (const auto &v : doc.array()) {
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

bool TranscriptStore::save(const QList<Transcript> &transcripts)
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
    const QByteArray json = QJsonDocument(arr).toJson(QJsonDocument::Indented);

    // Written to a temp file and only swapped in on commit(), so a failed
    // or interrupted write leaves the previous history intact.
    QSaveFile file(filePath());
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning("TranscriptStore: cannot write %s: %s",
                 qPrintable(file.fileName()), qPrintable(file.errorString()));
        return false;
    }
    // Transcripts are whatever the user dictated: notes, messages, possibly
    // credentials read aloud. Default file mode is world readable, so on a
    // shared machine any other local account could read them.
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (file.write(json) != json.size()) {
        qWarning("TranscriptStore: writing %s failed: %s",
                 qPrintable(file.fileName()), qPrintable(file.errorString()));
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        qWarning("TranscriptStore: saving %s failed: %s",
                 qPrintable(file.fileName()), qPrintable(file.errorString()));
        return false;
    }
    return true;
}
