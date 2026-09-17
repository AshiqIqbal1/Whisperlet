#ifndef MODELCATALOG_H
#define MODELCATALOG_H

#include <QList>
#include <QString>

// Static description of the whisper.cpp ggml models we let the user pick
// from in Settings. Nothing here is downloaded yet — see ModelManager.
struct ModelInfo
{
    QString id;          // "tiny", "base", "small", "medium", "large-v3-turbo"
    QString label;        // shown in the combo box
    QString description;  // shown as a subtitle / tooltip
    QString filename;     // "ggml-tiny.bin" — also the on-disk cache filename
    qint64  approxBytes = 0;  // for the download progress bar and free-space checks
    QString sha256;       // pinned upstream hash; downloads failing it are discarded
};

namespace ModelCatalog {

const QList<ModelInfo> &all();
const ModelInfo *find(const QString &id);

// Direct HTTPS link to the ggml file on the whisper.cpp Hugging Face repo.
QString downloadUrl(const ModelInfo &info);

QString humanSize(qint64 bytes);

} // namespace ModelCatalog

#endif // MODELCATALOG_H
