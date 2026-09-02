#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "transcriptcard.h"

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QList>
#include <QMainWindow>

#include <memory>
#include <vector>

class AudioRecorder;
class GlobalHotkey;
class ModelManager;
class QLabel;
class QLineEdit;
class QMediaPlayer;
class QAudioOutput;
class QTimer;
class QVBoxLayout;
class QWidget;
class RecordButton;
class RecordingPill;
class WhisperEngine;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void toggleRecording();
    void applyFilter(const QString &needle);
    void openSettings();

private:
    QWidget *buildHeader();
    QWidget *buildList();
    QWidget *buildFooter();

    // Runs whisper on a worker thread; samples must be 16kHz mono float32.
    // clipId is empty for fresh recordings (a new id is minted and the clip
    // saved), non-empty when re-transcribing an existing card's audio.
    // dictated = hotkey recording made while another app had focus; the
    // result is pasted into that app instead of only landing in the list.
    void runTranscription(std::vector<float> samples, int durationSec,
                          const QString &clipId, bool dictated);

    bool ensureModelReady(); // downloaded? if not, nudges user to Settings
    void refreshHint();
    void addCard(const Transcript &t, bool atTop);
    void removeTranscript(const QString &id);
    void playClip(const QString &id);
    void retranscribe(const QString &id);
    void persist();
    void refreshEmptyState();
    void flashStatus(const QString &message);

    // --- ui ---
    QLineEdit    *m_search = nullptr;
    QVBoxLayout  *m_listLayout = nullptr;
    QWidget      *m_emptyState = nullptr;
    RecordButton *m_record = nullptr;
    QLabel       *m_status = nullptr;
    QLabel       *m_hint = nullptr;
    QTimer       *m_statusTimer = nullptr;

    // --- engine & io ---
    ModelManager  *m_models = nullptr;
    AudioRecorder *m_recorder = nullptr;
    GlobalHotkey  *m_hotkey = nullptr;
    QMediaPlayer  *m_player = nullptr;
    QAudioOutput  *m_audioOut = nullptr;

    std::unique_ptr<WhisperEngine> m_engine;
    QFutureWatcher<QString> m_transcribeWatcher;
    QFutureWatcher<bool> m_preloadWatcher;
    RecordingPill *m_pill = nullptr;
    bool m_transcribing = false;
    bool m_dictating = false; // current recording started via hotkey, away from the app

    QElapsedTimer m_recordClock;
    QList<TranscriptCard *> m_cards;
};

#endif // MAINWINDOW_H
