#include "modelcatalog.h"

namespace {

// Sizes are the real ggml file sizes on huggingface.co/ggerganov/whisper.cpp
// as of this writing — close enough for a progress bar; the download itself
// always trusts the server's real Content-Length, this is only a fallback.
// sha256 values are the LFS oids from huggingface.co/ggerganov/whisper.cpp.
const QList<ModelInfo> kModels = {
    {QStringLiteral("tiny"),
     QStringLiteral("Tiny"),
     QStringLiteral("Fastest, least accurate. Good for quick drafts."),
     QStringLiteral("ggml-tiny.bin"),
     77'700'000,
     QStringLiteral("be07e048e1e599ad46341c8d2a135645097a538221678b7acdd1b1919c6e1b21")},
    {QStringLiteral("base"),
     QStringLiteral("Base"),
     QStringLiteral("Small step up from Tiny, still very fast."),
     QStringLiteral("ggml-base.bin"),
     148'000'000,
     QStringLiteral("60ed5bc3dd14eea856493d334349b405782ddcaf0028d4b5df4088345fba2efe")},
    {QStringLiteral("small"),
     QStringLiteral("Small"),
     QStringLiteral("Good balance of speed and accuracy."),
     QStringLiteral("ggml-small.bin"),
     488'000'000,
     QStringLiteral("1be3a9b2063867b937e64e2ec7483364a79917e157fa98c5d94b5c1fffea987b")},
    {QStringLiteral("medium"),
     QStringLiteral("Medium"),
     QStringLiteral("Noticeably more accurate, noticeably slower."),
     QStringLiteral("ggml-medium.bin"),
     1'534'000'000,
     QStringLiteral("6c14d5adee5f86394037b4e4e8b59f1673b6cee10e3cf0b11bbdbee79c156208")},
    {QStringLiteral("large-v3-turbo"),
     QStringLiteral("Large (v3 turbo)"),
     QStringLiteral("Best accuracy. Slow on modest hardware."),
     QStringLiteral("ggml-large-v3-turbo.bin"),
     1'625'000'000,
     QStringLiteral("1fc70f774d38eb169993ac391eea357ef47c88757ef72ee5943879b7e8e2bc69")},
};

} // namespace

const QList<ModelInfo> &ModelCatalog::all()
{
    return kModels;
}

const ModelInfo *ModelCatalog::find(const QString &id)
{
    for (const ModelInfo &m : kModels) {
        if (m.id == id)
            return &m;
    }
    return nullptr;
}

QString ModelCatalog::downloadUrl(const ModelInfo &info)
{
    return QStringLiteral("https://huggingface.co/ggerganov/whisper.cpp/resolve/main/%1")
        .arg(info.filename);
}

QString ModelCatalog::humanSize(qint64 bytes)
{
    const double gb = bytes / 1000.0 / 1000.0 / 1000.0;
    if (gb >= 1.0)
        return QStringLiteral("%1 GB").arg(gb, 0, 'f', 1);
    const double mb = bytes / 1000.0 / 1000.0;
    return QStringLiteral("%1 MB").arg(mb, 0, 'f', 0);
}

bool ModelCatalog::isWellFormedSha256(const QString &hash)
{
    if (hash.size() != 64)
        return false;
    for (const QChar c : hash) {
        if (!((c >= u'0' && c <= u'9') || (c >= u'a' && c <= u'f')))
            return false;
    }
    return true;
}
