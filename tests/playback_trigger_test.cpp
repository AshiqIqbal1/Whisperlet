// Regression test for the play-button trigger path: a TranscriptCard whose
// clip is on disk emits playRequested(id) when its play button is clicked,
// and that id resolves through AudioClipStore to a WAV file QMediaPlayer can
// actually load and play — the same two steps MainWindow::playClip chains
// together. Building the full MainWindow isn't practical here: its
// RecordingPill construction talks to a real native NSWindow overlay
// (overlaywindow_mac.mm) that segfaults under the offscreen QPA platform
// this test runs under, so this test exercises the card's signal and the
// AudioClipStore/QMediaPlayer path directly instead.

#include "audioclipstore.h"
#include "transcriptcard.h"

#include <QAudioOutput>
#include <QApplication>
#include <QMediaPlayer>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>
#include <QToolButton>
#include <QUrl>

#include <cmath>
#include <cstdio>

namespace {
int failures = 0;

void check(bool condition, const char *what)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}
} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    // Distinct org/app name so this test's data dir never touches the real
    // user's Whisperlet data.
    QCoreApplication::setOrganizationName(QStringLiteral("WhisperletTest"));
    QCoreApplication::setApplicationName(QStringLiteral("PlaybackTriggerTest"));

    const QString id = QStringLiteral("playback-trigger-test");
    std::vector<float> samples(16000 * 2);
    for (size_t i = 0; i < samples.size(); ++i)
        samples[i] = 0.3f * std::sin(2.0 * M_PI * 440.0 * double(i) / 16000.0);
    check(AudioClipStore::save(id, samples, 16000), "AudioClipStore::save wrote the seeded clip");
    check(AudioClipStore::exists(id), "AudioClipStore::exists sees the seeded clip");

    Transcript t{id, QStringLiteral("hello world"), QDateTime::currentDateTime(), 2};
    TranscriptCard card(t);
    card.setAudioAvailable(AudioClipStore::exists(id));
    card.show(); // isVisible() reflects the shown ancestor chain a real card has in the list

    auto *playButton = card.findChild<QToolButton *>(QStringLiteral("playBtn"));
    check(playButton != nullptr, "the card's play button exists");
    check(playButton && playButton->isVisible(), "the play button is visible when audio is kept");

    QSignalSpy spy(&card, &TranscriptCard::playRequested);
    if (playButton)
        QTest::mouseClick(playButton, Qt::LeftButton);
    check(spy.count() == 1, "clicking play emits playRequested exactly once");
    check(!spy.isEmpty() && spy.constFirst().value(0).toString() == id,
          "playRequested carries the card's transcript id");

    // Mirror MainWindow::playClip's exact steps against the id the click
    // just emitted, so a mismatch between what AudioClipStore wrote and
    // what QMediaPlayer can load would fail this test.
    check(AudioClipStore::exists(id), "the clip the click refers to is still on disk");
    QMediaPlayer player;
    QAudioOutput out;
    player.setAudioOutput(&out);
    bool sawError = false;
    QObject::connect(&player, &QMediaPlayer::errorOccurred,
                      [&sawError](QMediaPlayer::Error, const QString &) { sawError = true; });
    player.setSource(QUrl::fromLocalFile(AudioClipStore::path(id)));
    player.play();
    QTest::qWait(500);
    check(!sawError, "QMediaPlayer reports no error loading the saved clip");
    check(player.mediaStatus() != QMediaPlayer::NoMedia
              && player.mediaStatus() != QMediaPlayer::InvalidMedia,
          "QMediaPlayer actually loaded the saved clip as playable media");

    AudioClipStore::remove(id);

    if (failures == 0)
        std::printf("PASS: playback_trigger_test\n");
    return failures == 0 ? 0 : 1;
}
