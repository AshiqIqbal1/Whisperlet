#ifndef UPDATECHECKER_H
#define UPDATECHECKER_H

#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;

// Points the user at the latest GitHub release when it's newer than the
// running build. Never downloads or installs anything, and never leaves the
// caller waiting forever — a request that doesn't finish within the timeout
// is reported as a failure like any other. Version comparison lives in
// VersionCompare so it can be tested without a network round trip.
class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    explicit UpdateChecker(QObject *parent = nullptr);

    // Test-only seam: points check() at a stand-in server instead of the
    // real GitHub API, so the checking/success/failure state machine can be
    // exercised deterministically. Production code never calls this.
    void setEndpointForTesting(const QUrl &url);

public slots:
    void check();

signals:
    void updateAvailable(const QString &version, const QString &releaseUrl);
    void upToDate();
    void checkFailed(const QString &error);

private:
    QNetworkAccessManager *m_net = nullptr;
    QUrl m_endpoint;
};

#endif // UPDATECHECKER_H
