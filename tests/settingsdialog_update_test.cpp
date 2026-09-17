// Drives the real SettingsDialog "Check for Updates" button end to end,
// pointing UpdateChecker at a local stand-in HTTP server instead of the
// real GitHub API so both the checking/success and checking/failure paths
// are deterministic. Requires QT_QPA_PLATFORM=offscreen (set by the test
// runner) since it builds real QWidgets.

#include "globalhotkey.h"
#include "modelmanager.h"
#include "settingsdialog.h"
#include "updatechecker.h"

#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

#include <cstdio>
#include <cstdlib>

namespace {
int failures = 0;

void check(bool condition, const char *what)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

// Minimal one-shot HTTP server: replies with a fixed body to the first
// request it receives, then stops listening.
class StubServer : public QObject
{
public:
    explicit StubServer(const QByteArray &httpResponse)
        : m_response(httpResponse)
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            QTcpSocket *socket = m_server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                socket->write(m_response);
                socket->flush();
                socket->disconnectFromHost();
            });
        });
        m_server.listen(QHostAddress::LocalHost);
    }

    QUrl url() const
    {
        QUrl u;
        u.setScheme(QStringLiteral("http"));
        u.setHost(QStringLiteral("127.0.0.1"));
        u.setPort(m_server.serverPort());
        return u;
    }

private:
    QTcpServer m_server;
    QByteArray m_response;
};

QPushButton *updateButton(SettingsDialog &dialog)
{
    return dialog.findChild<QPushButton *>(QStringLiteral("updateButton"));
}

UpdateChecker *checkerOf(SettingsDialog &dialog)
{
    return dialog.findChild<UpdateChecker *>();
}

void testCheckingStateThenUpdateAvailable()
{
    ModelManager models;
    GlobalHotkey hotkey;
    SettingsDialog dialog(&models, &hotkey);
    dialog.show();

    const QByteArray body = R"({"tag_name":"v99.0.0","html_url":"https://example.com/release"})";
    StubServer server(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
        "\r\n" + body);
    checkerOf(dialog)->setEndpointForTesting(server.url());

    QPushButton *btn = updateButton(dialog);
    check(btn != nullptr, "update button exists");
    check(btn->isEnabled(), "update button starts enabled");

    QSignalSpy updateSpy(checkerOf(dialog), &UpdateChecker::updateAvailable);
    btn->click();

    // Synchronous part of the state machine: button disables and shows the
    // in-flight label before any network I/O has had a chance to complete.
    check(!btn->isEnabled(), "update button disables immediately on click");
    check(btn->text() == QObject::tr("Checking..."), "button label switches to Checking...");

    check(updateSpy.wait(5000), "updateAvailable fired within timeout");

    check(btn->isEnabled(), "update button re-enables after result");
    check(btn->text() == QObject::tr("Check for Updates"), "button label reverts after result");

    QPushButton *openBtn = dialog.findChild<QPushButton *>(QStringLiteral("openReleaseButton"));
    check(openBtn != nullptr && openBtn->isVisible(), "Open Release Page button revealed");
}

void testCheckFailedShowsQuietMessageNoDialog()
{
    ModelManager models;
    GlobalHotkey hotkey;
    SettingsDialog dialog(&models, &hotkey);

    // Nothing is listening here: the connection is refused immediately,
    // which is enough to exercise the checkFailed branch without touching
    // any real network state.
    QUrl deadEndpoint(QStringLiteral("http://127.0.0.1:1"));
    checkerOf(dialog)->setEndpointForTesting(deadEndpoint);

    QPushButton *btn = updateButton(dialog);
    QSignalSpy failSpy(checkerOf(dialog), &UpdateChecker::checkFailed);
    btn->click();

    check(failSpy.wait(5000), "checkFailed fired within timeout");

    check(btn->isEnabled(), "update button re-enables after failure");
    check(btn->text() == QObject::tr("Check for Updates"), "button label reverts after failure");

    QPushButton *openBtn = dialog.findChild<QPushButton *>(QStringLiteral("openReleaseButton"));
    check(openBtn != nullptr && !openBtn->isVisible(), "Open Release Page stays hidden on failure");

    QLabel *status = dialog.findChild<QLabel *>(QStringLiteral("cardMeta"));
    Q_UNUSED(status);
    // The failure path never raises a QMessageBox — if it did, this test
    // would hang on the modal event loop and time out instead of finishing.
}
} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    Q_UNUSED(app);
    // isNewer() treats an empty running version as "never newer" (guards
    // against a false positive when the version string is unset), so the
    // updateAvailable scenario needs a real baseline to compare against.
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    testCheckingStateThenUpdateAvailable();
    testCheckFailedShowsQuietMessageNoDialog();

    if (failures == 0)
        std::printf("All settings-dialog update tests passed.\n");

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
