#include "updatechecker.h"

#include "versioncompare.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace {
constexpr auto kLatestReleaseUrl =
    "https://api.github.com/repos/AshiqIqbal1/Whisperlet/releases/latest";
// GitHub is generally quick to answer; this just guarantees the button never
// shows "Checking..." forever on a stalled connection.
constexpr int kTimeoutMs = 8000;
}

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
    , m_endpoint(QString::fromLatin1(kLatestReleaseUrl))
{
}

void UpdateChecker::setEndpointForTesting(const QUrl &url)
{
    m_endpoint = url;
}

void UpdateChecker::check()
{
    QNetworkRequest request{m_endpoint};
    request.setTransferTimeout(kTimeoutMs);
    // The GitHub API rejects requests with no User-Agent.
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Whisperlet-UpdateChecker"));

    QNetworkReply *reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit checkFailed(reply->errorString());
            return;
        }

        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        const QString tag = obj.value(QStringLiteral("tag_name")).toString();
        const QString releaseUrl = obj.value(QStringLiteral("html_url")).toString();

        if (tag.isEmpty() || releaseUrl.isEmpty()) {
            emit checkFailed(tr("Unexpected response from GitHub."));
            return;
        }

        if (VersionCompare::isNewer(tag, QCoreApplication::applicationVersion()))
            emit updateAvailable(tag, releaseUrl);
        else
            emit upToDate();
    });
}
