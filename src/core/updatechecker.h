#ifndef UPDATECHECKER_H
#define UPDATECHECKER_H

#include <QObject>
#include <QString>

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

public slots:
    void check();

signals:
    void updateAvailable(const QString &version, const QString &releaseUrl);
    void upToDate();
    void checkFailed(const QString &error);

private:
    QNetworkAccessManager *m_net = nullptr;
};

#endif // UPDATECHECKER_H
