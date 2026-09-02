#include "mainwindow.h"

#include "audioclipstore.h"
#include "audiofiledecoder.h"
#include "audiorecorder.h"
#include "globalhotkey.h"
#include "icons.h"
#include "modelcatalog.h"
#include "modelmanager.h"
#include "recordbutton.h"
#include "recordingpill.h"
#include "settingsdialog.h"
#include "textinjector.h"
#include "theme.h"
#include "titlebar.h"
#include "transcriptstore.h"
#include "whisperengine.h"

#include <QApplication>
#include <QAudioDevice>
#include <QAudioOutput>
#include <QClipboard>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QMenu>
#include <QMimeData>
#include <QScrollArea>
#include <QSettings>
#include <QStatusBar>
#include <QTimer>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace {
constexpr int kWindowW = 560;
constexpr int kWindowH = 760;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_engine(std::make_unique<WhisperEngine>())
{
    setWindowTitle(tr("Whisperlet"));
    resize(kWindowW, kWindowH);
    setAcceptDrops(true);
    setStyleSheet(Theme::styleSheet());

    // Custom chrome: hide the OS title bar, TitleBar below provides logo,
    // drag area and window buttons. The status bar's size grip keeps the
    // window resizable. Translucency lets #root paint rounded corners.
    setWindowFlag(Qt::FramelessWindowHint, true);
    setAttribute(Qt::WA_TranslucentBackground, true);

    m_models = new ModelManager(this);
    m_recorder = new AudioRecorder(this);
    connect(m_recorder, &AudioRecorder::levelChanged, this,
            [this](qreal level) { m_record->setLevel(level); });

    m_player = new QMediaPlayer(this);
    m_audioOut = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOut);

    auto *root = new QWidget(this);
    root->setObjectName(QStringLiteral("root"));
    auto *outer = new QVBoxLayout(root);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(new TitleBar(root));

    auto *content = new QWidget(root);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(20, 6, 20, 16);
    layout->setSpacing(14);

    layout->addWidget(buildHeader());
    layout->addWidget(buildList(), /*stretch=*/1);
    layout->addWidget(buildFooter());

    outer->addWidget(content, /*stretch=*/1);
    setCentralWidget(root);

    m_pill = new RecordingPill; // top-level tool window, parentless on purpose

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("statusLabel"));
    statusBar()->addWidget(m_status);
    // The status bar sits BELOW the central widget, so it owns the window's
    // bottom corners — round them here, #root rounds only the top pair.
    statusBar()->setStyleSheet(QStringLiteral(
        "QStatusBar{background:#141416;border-top:1px solid #26262A;"
        "border-bottom-left-radius:12px;border-bottom-right-radius:12px;}"));

    m_statusTimer = new QTimer(this);
    m_statusTimer->setSingleShot(true);
    connect(m_statusTimer, &QTimer::timeout, this, [this] { m_status->clear(); });

    connect(&m_transcribeWatcher, &QFutureWatcher<QString>::finished, this, [this] {
        m_transcribing = false;
        m_pill->hide();
        const QString text = m_transcribeWatcher.result();
        if (text.isEmpty()) {
            flashStatus(tr("Transcription failed: %1").arg(m_engine->lastError()));
            return;
        }

        const auto props = m_transcribeWatcher.property("job").toMap();
        const QString clipId = props.value(QStringLiteral("clipId")).toString();
        const int durationSec = props.value(QStringLiteral("durationSec")).toInt();
        const bool isRetry = props.value(QStringLiteral("isRetry")).toBool();
        const bool dictated = props.value(QStringLiteral("dictated")).toBool();

        // Dictation: drop the text into whatever app the user is in.
        if (dictated && QSettings().value(QStringLiteral("pasteAfterDictation"), true).toBool()) {
            if (TextInjector::canInject()) {
                TextInjector::pasteIntoActiveApp(text);
            } else {
                TextInjector::requestPermission(); // mac: shows Accessibility prompt
                flashStatus(tr("Grant Accessibility permission to let Whisperlet "
                               "type into other apps — text was copied instead"));
                QGuiApplication::clipboard()->setText(text);
            }
        }

        if (isRetry) {
            // Update the existing card in place: delete + re-add keeps it simple.
            for (auto *card : std::as_const(m_cards)) {
                if (card->data().id == clipId) {
                    Transcript updated = card->data();
                    updated.text = text;
                    m_cards.removeOne(card);
                    card->deleteLater();
                    addCard(updated, true);
                    break;
                }
            }
        } else {
            addCard({clipId, text, QDateTime::currentDateTime(), durationSec}, true);
        }
        persist();
        // Show the real timing so slowness is diagnosable, not mysterious.
        flashStatus(tr("Done — transcribed in %1s")
                        .arg(m_engine->lastTranscribeMs() / 1000.0, 0, 'f', 1));
    });

    // Global hotkey — works even when another app has focus. Combo is
    // user-configurable in Settings; saved as a portable key string.
    //
    // Pressed while another app is focused = dictation: record WITHOUT
    // raising our window (the target text field must keep focus), show the
    // floating pill, and paste the result into that app when done.
    m_hotkey = new GlobalHotkey(this);
    connect(m_hotkey, &GlobalHotkey::activated, this, [this] {
        if (!m_recorder->isRecording())
            m_dictating = !isActiveWindow();
        toggleRecording();
    });
    QSettings settings;
    if (settings.value(QStringLiteral("hotkeyMode")).toString() == QStringLiteral("tap")) {
        const auto key = GlobalHotkey::ModKey(
            settings.value(QStringLiteral("modTapKey"), int(GlobalHotkey::ModKey::RightCmd)).toInt());
        if (!m_hotkey->setModifierTap(key))
            m_hotkey->setSequence(GlobalHotkey::defaultSequence()); // e.g. Accessibility not granted yet
    } else {
        const QString savedCombo = settings.value(QStringLiteral("globalHotkey")).toString();
        const QKeySequence combo = savedCombo.isEmpty()
            ? GlobalHotkey::defaultSequence()
            : QKeySequence(savedCombo, QKeySequence::PortableText);
        if (!m_hotkey->setSequence(combo) && !m_hotkey->setSequence(GlobalHotkey::defaultSequence()))
            flashStatus(tr("Global hotkey unavailable (in use by another app)"));
    }
    refreshHint();

    // Restore previous sessions' transcripts.
    const QList<Transcript> saved = TranscriptStore::load();
    for (const Transcript &t : saved)
        addCard(t, false);

    refreshEmptyState();

    // Preload the active model in the background. Without this the first
    // transcription silently pays the full model load (large-v3-turbo is a
    // 1.6GB read — many seconds on a laptop) and looks like the app hung.
    // m_transcribing gates transcription until the engine is ready.
    connect(&m_preloadWatcher, &QFutureWatcher<bool>::finished, this, [this] {
        m_transcribing = false;
        if (m_preloadWatcher.result())
            flashStatus(tr("Model ready (loaded in %1s)")
                            .arg(m_engine->lastLoadMs() / 1000.0, 0, 'f', 1));
    });
    if (m_models->isDownloaded(m_models->activeModelId())) {
        const QString path = m_models->localPath(m_models->activeModelId());
        m_transcribing = true;
        flashStatus(tr("Loading model…"));
        WhisperEngine *engine = m_engine.get();
        m_preloadWatcher.setFuture(QtConcurrent::run(
            [engine, path] { return engine->loadModel(path); }));
    }
}

MainWindow::~MainWindow()
{
    delete m_pill; // parentless top-level window, not in our child tree
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Let workers finish before the engine is torn down.
    if (m_preloadWatcher.isRunning())
        m_preloadWatcher.waitForFinished();
    if (m_transcribeWatcher.isRunning())
        m_transcribeWatcher.waitForFinished();
    persist();
    QMainWindow::closeEvent(event);
}

QWidget *MainWindow::buildHeader()
{
    auto *wrap = new QWidget(this);
    auto *row = new QHBoxLayout(wrap);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(10);

    auto *searchIcon = new QLabel(wrap);
    searchIcon->setPixmap(Icons::icon(Icons::Search, Theme::TextFaint, 15).pixmap(15, 15));

    m_search = new QLineEdit(wrap);
    m_search->setObjectName(QStringLiteral("searchBar"));
    m_search->setPlaceholderText(tr("Search in transcriptions"));
    m_search->setClearButtonEnabled(true);
    connect(m_search, &QLineEdit::textChanged, this, &MainWindow::applyFilter);

    // Icon lives visually inside the field via a leading-margin trick.
    m_search->setTextMargins(24, 0, 0, 0);
    searchIcon->setParent(m_search);
    searchIcon->move(12, 11);

    row->addWidget(m_search, 1);
    return wrap;
}

QWidget *MainWindow::buildList()
{
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *body = new QWidget(scroll);
    body->setObjectName(QStringLiteral("scrollBody"));
    m_listLayout = new QVBoxLayout(body);
    m_listLayout->setContentsMargins(2, 2, 2, 2);
    m_listLayout->setSpacing(12);
    m_listLayout->addStretch(1); // keeps cards top-aligned as list grows/shrinks

    m_emptyState = new QWidget(body);
    auto *emptyLay = new QVBoxLayout(m_emptyState);
    emptyLay->setContentsMargins(0, 60, 0, 0);
    auto *emptyLabel = new QLabel(tr("No transcriptions yet\nPress record to get started"), m_emptyState);
    emptyLabel->setObjectName(QStringLiteral("emptyTitle"));
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLay->addWidget(emptyLabel);
    m_listLayout->insertWidget(0, m_emptyState);

    scroll->setWidget(body);
    return scroll;
}

QWidget *MainWindow::buildFooter()
{
    auto *wrap = new QWidget(this);
    auto *outer = new QVBoxLayout(wrap);
    outer->setContentsMargins(0, 4, 0, 0);
    outer->setSpacing(10);

    m_record = new RecordButton(wrap);
    connect(m_record, &QAbstractButton::clicked, this, &MainWindow::toggleRecording);

    auto *centerRow = new QHBoxLayout;
    centerRow->addStretch(1);
    centerRow->addWidget(m_record);
    centerRow->addStretch(1);
    outer->addLayout(centerRow);

    auto *bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(8);

    m_hint = new QLabel(wrap);
    m_hint->setObjectName(QStringLiteral("hint"));
    bottomRow->addWidget(m_hint);
    bottomRow->addStretch(1);

    auto *mic = new QToolButton(wrap);
    mic->setObjectName(QStringLiteral("footerBtn"));
    mic->setIcon(Icons::icon(Icons::Mic, Theme::TextMuted, 16));
    mic->setToolTip(tr("Input device"));
    connect(mic, &QToolButton::clicked, this, [this, mic] {
        // Fresh menu on every click — device list changes as mics (un)plug.
        QMenu menu(this);
        const QByteArray currentId = QSettings().value(QStringLiteral("inputDeviceId")).toByteArray();

        auto *systemDefault = menu.addAction(tr("System default"));
        systemDefault->setCheckable(true);
        systemDefault->setChecked(currentId.isEmpty());
        connect(systemDefault, &QAction::triggered, this, [this] {
            QSettings().remove(QStringLiteral("inputDeviceId"));
            flashStatus(tr("Microphone: system default"));
        });

        const auto devices = QMediaDevices::audioInputs();
        for (const QAudioDevice &d : devices) {
            auto *act = menu.addAction(d.description());
            act->setCheckable(true);
            act->setChecked(d.id() == currentId);
            connect(act, &QAction::triggered, this, [this, d] {
                QSettings().setValue(QStringLiteral("inputDeviceId"), d.id());
                flashStatus(tr("Microphone: %1").arg(d.description()));
            });
        }
        menu.exec(mic->mapToGlobal(QPoint(0, -menu.sizeHint().height() - 4)));
    });
    bottomRow->addWidget(mic);

    auto *trash = new QToolButton(wrap);
    trash->setObjectName(QStringLiteral("footerBtn"));
    trash->setIcon(Icons::icon(Icons::Trash, Theme::TextMuted, 16));
    trash->setToolTip(tr("Clear all"));
    connect(trash, &QToolButton::clicked, this, [this] {
        for (auto *card : std::as_const(m_cards)) {
            AudioClipStore::remove(card->data().id);
            card->deleteLater();
        }
        m_cards.clear();
        persist();
        refreshEmptyState();
        flashStatus(tr("Cleared"));
    });
    bottomRow->addWidget(trash);

    auto *settings = new QToolButton(wrap);
    settings->setObjectName(QStringLiteral("footerBtn"));
    settings->setIcon(Icons::icon(Icons::Settings, Theme::TextMuted, 16));
    settings->setToolTip(tr("Settings"));
    connect(settings, &QToolButton::clicked, this, &MainWindow::openSettings);
    bottomRow->addWidget(settings);

    outer->addLayout(bottomRow);
    return wrap;
}

void MainWindow::openSettings()
{
    // Release the OS hotkey registration while the dialog is open. A
    // registered combo is consumed by the OS before the shortcut editor
    // field ever sees it (worst on Windows, where WM_HOTKEY eats the
    // keystroke entirely) — which made the editor look like it kept
    // resetting to the old combo. setSequence() during suspension just
    // stores the combo; resume() re-registers whatever is current.
    m_hotkey->suspend();
    SettingsDialog dialog(m_models, m_hotkey, this);
    dialog.exec();
    if (!m_hotkey->resume())
        flashStatus(tr("Shortcut %1 is in use by another app — pick a different one")
                        .arg(m_hotkey->comboLabel()));
    refreshHint(); // combo may have changed
}

void MainWindow::refreshHint()
{
    m_hint->setText(tr("Drop audio file here to transcribe  ·  %1 to record")
                        .arg(m_hotkey->comboLabel()));
}

bool MainWindow::ensureModelReady()
{
    const QString id = m_models->activeModelId();
    if (m_models->isDownloaded(id))
        return true;

    flashStatus(tr("Model \"%1\" is not downloaded yet — opening Settings").arg(id));
    openSettings();
    return m_models->isDownloaded(m_models->activeModelId());
}

void MainWindow::toggleRecording()
{
    if (m_transcribing) {
        m_dictating = false; // this attempt is over; don't leak into the next one
        flashStatus(m_preloadWatcher.isRunning()
                        ? tr("Model still loading…")
                        : tr("Still transcribing the previous recording…"));
        return;
    }

    if (!m_recorder->isRecording()) {
        if (!ensureModelReady()) {
            m_dictating = false;
            return;
        }
        if (!m_recorder->start()) {
            flashStatus(m_recorder->lastError());
            m_dictating = false;
            return;
        }
        m_recordClock.start();
        m_record->setRecording(true);
        if (m_dictating)
            m_pill->showRecording();
        flashStatus(tr("Recording…"));
    } else {
        std::vector<float> samples = m_recorder->stop();
        m_record->setRecording(false);

        const bool dictated = m_dictating;
        m_dictating = false;

        const int durationSec = int(m_recordClock.elapsed() / 1000);
        if (samples.size() < 16000 / 2) { // < 0.5s of audio — accidental tap
            m_pill->hide();
            flashStatus(tr("Recording too short"));
            return;
        }
        if (dictated)
            m_pill->showTranscribing();
        runTranscription(std::move(samples), durationSec, QString(), dictated);
    }
}

void MainWindow::runTranscription(std::vector<float> samples, int durationSec,
                                  const QString &clipId, bool dictated)
{
    const QString id = clipId.isEmpty()
        ? QUuid::createUuid().toString(QUuid::WithoutBraces).left(8)
        : clipId;

    if (clipId.isEmpty())
        AudioClipStore::save(id, samples);

    const QString modelPath = m_models->localPath(m_models->activeModelId());

    QVariantMap job;
    job[QStringLiteral("clipId")] = id;
    job[QStringLiteral("durationSec")] = durationSec;
    job[QStringLiteral("isRetry")] = !clipId.isEmpty();
    job[QStringLiteral("dictated")] = dictated;
    m_transcribeWatcher.setProperty("job", job);

    m_transcribing = true;
    flashStatus(tr("Transcribing…"));

    // WhisperEngine is only ever touched from inside this task while
    // m_transcribing guards against a second one starting.
    WhisperEngine *engine = m_engine.get();
    m_transcribeWatcher.setFuture(QtConcurrent::run(
        [engine, modelPath, samples = std::move(samples)]() -> QString {
            if (engine->loadedPath() != modelPath) {
                if (!engine->loadModel(modelPath))
                    return QString();
            }
            return engine->transcribe(samples, QStringLiteral("en"));
        }));
}

void MainWindow::addCard(const Transcript &t, bool atTop)
{
    auto *card = new TranscriptCard(t, this);
    connect(card, &TranscriptCard::deleteRequested, this, &MainWindow::removeTranscript);
    connect(card, &TranscriptCard::copyRequested, this, [this](const QString &id) {
        for (auto *c : std::as_const(m_cards)) {
            if (c->data().id == id) {
                QGuiApplication::clipboard()->setText(c->data().text);
                flashStatus(tr("Copied to clipboard"));
                break;
            }
        }
    });
    connect(card, &TranscriptCard::retryRequested, this, &MainWindow::retranscribe);
    connect(card, &TranscriptCard::playRequested, this, &MainWindow::playClip);

    const int insertPos = atTop ? 1 : m_listLayout->count() - 1; // slot 0 is the empty state
    m_listLayout->insertWidget(insertPos, card);
    atTop ? m_cards.prepend(card) : m_cards.append(card);

    refreshEmptyState();
}

void MainWindow::playClip(const QString &id)
{
    if (!AudioClipStore::exists(id)) {
        flashStatus(tr("No audio kept for this transcript"));
        return;
    }
    m_player->stop();
    m_player->setSource(QUrl::fromLocalFile(AudioClipStore::path(id)));
    m_player->play();
}

void MainWindow::retranscribe(const QString &id)
{
    if (m_transcribing) {
        flashStatus(tr("Still transcribing the previous recording…"));
        return;
    }
    if (!ensureModelReady())
        return;

    std::vector<float> samples = AudioClipStore::load(id);
    if (samples.empty()) {
        flashStatus(tr("No audio kept for this transcript"));
        return;
    }

    int durationSec = 0;
    for (auto *card : std::as_const(m_cards)) {
        if (card->data().id == id) {
            durationSec = card->data().durationSec;
            break;
        }
    }
    runTranscription(std::move(samples), durationSec, id, /*dictated=*/false);
}

void MainWindow::removeTranscript(const QString &id)
{
    for (int i = 0; i < m_cards.size(); ++i) {
        if (m_cards[i]->data().id == id) {
            m_cards[i]->deleteLater();
            m_cards.removeAt(i);
            break;
        }
    }
    AudioClipStore::remove(id);
    persist();
    refreshEmptyState();
    flashStatus(tr("Deleted"));
}

void MainWindow::persist()
{
    QList<Transcript> list;
    list.reserve(m_cards.size());
    for (auto *card : std::as_const(m_cards))
        list.append(card->data());
    TranscriptStore::save(list);
}

void MainWindow::applyFilter(const QString &needle)
{
    for (auto *card : std::as_const(m_cards))
        card->setVisible(card->matches(needle));
    refreshEmptyState();
}

void MainWindow::refreshEmptyState()
{
    m_emptyState->setVisible(m_cards.isEmpty());
}

void MainWindow::flashStatus(const QString &message)
{
    m_status->setText(message);
    m_statusTimer->start(3000);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !m_search->hasFocus()) {
        toggleRecording();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const auto urls = event->mimeData()->urls();
    if (urls.isEmpty() || !urls.first().isLocalFile())
        return;
    if (m_transcribing) {
        flashStatus(tr("Still transcribing the previous recording…"));
        return;
    }
    if (!ensureModelReady())
        return;

    flashStatus(tr("Decoding %1…").arg(urls.first().fileName()));

    auto *decoder = new AudioFileDecoder(urls.first().toLocalFile(), this);
    connect(decoder, &AudioFileDecoder::finished, this,
            [this](std::vector<float> samples, const QString &error) {
                if (!error.isEmpty() || samples.empty()) {
                    flashStatus(tr("Could not decode file: %1").arg(error));
                    return;
                }
                const int durationSec = int(samples.size() / 16000);
                runTranscription(std::move(samples), durationSec, QString(), /*dictated=*/false);
            });
    decoder->start();
}
