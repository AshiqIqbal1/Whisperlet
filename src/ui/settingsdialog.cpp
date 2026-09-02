#include "settingsdialog.h"

#include "globalhotkey.h"
#include "modelcatalog.h"
#include "modelmanager.h"
#include "textinjector.h"
#include "theme.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(ModelManager *models, GlobalHotkey *hotkey, QWidget *parent)
    : QDialog(parent)
    , m_models(models)
    , m_hotkey(hotkey)
{
    setWindowTitle(tr("Settings"));
    setMinimumWidth(520);
    setStyleSheet(Theme::styleSheet() + QStringLiteral(R"(
QDialog { background: #1A1A1D; }
#sectionHeading { font-size: 16px; font-weight: 600; padding-top: 4px; }
QRadioButton { spacing: 8px; font-size: 14px; }
QRadioButton:disabled { color: #5E5E66; }
QProgressBar {
    background: #26262A;
    border: none;
    border-radius: 4px;
    height: 8px;
    text-align: center;
    font-size: 10px;
    color: transparent;
}
QProgressBar::chunk { background: #0A84FF; border-radius: 4px; }
QPushButton {
    background: #2A2A30;
    border: 1px solid #3A3A42;
    border-radius: 8px;
    padding: 5px 14px;
    font-size: 13px;
}
QPushButton:hover { background: #34343C; }
QPushButton:disabled { color: #5E5E66; background: #222226; }
)"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 16);
    layout->setSpacing(14);

    auto *heading = new QLabel(tr("Transcription model"), this);
    heading->setObjectName(QStringLiteral("sectionHeading"));
    layout->addWidget(heading);

    auto *sub = new QLabel(tr("Models are downloaded once and stored locally. "
                              "Larger models are more accurate but slower."), this);
    sub->setObjectName(QStringLiteral("cardMeta"));
    sub->setWordWrap(true);
    layout->addWidget(sub);

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(10);
    grid->setColumnStretch(1, 1);

    // QRadioButtons auto-group per parent widget — without explicit groups
    // the model radios and the shortcut-mode radios would all be mutually
    // exclusive with EACH OTHER (picking a shortcut mode deselected the
    // active model).
    auto *modelGroup = new QButtonGroup(this);

    int row = 0;
    for (const ModelInfo &info : ModelCatalog::all()) {
        Row r;

        r.active = new QRadioButton(info.label, this);
        modelGroup->addButton(r.active);
        r.active->setToolTip(info.description);
        connect(r.active, &QRadioButton::toggled, this, [this, id = info.id](bool on) {
            if (on)
                m_models->setActiveModelId(id);
        });
        grid->addWidget(r.active, row, 0, Qt::AlignVCenter);

        r.size = new QLabel(ModelCatalog::humanSize(info.approxBytes), this);
        r.size->setObjectName(QStringLiteral("cardMeta"));
        grid->addWidget(r.size, row, 1, Qt::AlignRight | Qt::AlignVCenter);

        r.progress = new QProgressBar(this);
        r.progress->setRange(0, 100);
        r.progress->setVisible(false);
        r.progress->setFixedWidth(110);
        grid->addWidget(r.progress, row, 2, Qt::AlignVCenter);

        r.action = new QPushButton(this);
        r.action->setFixedWidth(96);
        connect(r.action, &QPushButton::clicked, this, [this, id = info.id] {
            if (m_models->isDownloading(id))
                m_models->cancelDownload(id);
            else if (m_models->isDownloaded(id))
                onDeleteClicked(id);
            else
                onDownloadClicked(id);
        });
        grid->addWidget(r.action, row, 3);

        m_rows.insert(info.id, r);
        ++row;
    }
    layout->addLayout(grid);

    // --- global shortcut ---------------------------------------------------
    auto *hotkeyHeading = new QLabel(tr("Global shortcut"), this);
    hotkeyHeading->setObjectName(QStringLiteral("sectionHeading"));
    layout->addWidget(hotkeyHeading);

    // Two trigger styles: a key combination, or a single right-side
    // modifier tapped on its own (OpenSuperWhisper style).
    m_comboRadio = new QRadioButton(tr("Key combination"), this);
    m_tapRadio = new QRadioButton(tr("Single modifier key"), this);
    auto *modeGroup = new QButtonGroup(this);
    modeGroup->addButton(m_comboRadio);
    modeGroup->addButton(m_tapRadio);
    (m_hotkey->isModifierTapMode() ? m_tapRadio : m_comboRadio)->setChecked(true);

    auto *comboRow = new QHBoxLayout;
    comboRow->setSpacing(10);
    comboRow->addWidget(m_comboRadio);

    m_hotkeyEdit = new QKeySequenceEdit(this);
    m_hotkeyEdit->setMaximumSequenceLength(1);
    m_hotkeyEdit->setClearButtonEnabled(true);
    m_hotkeyEdit->setKeySequence(m_hotkey->sequence());
    m_hotkeyEdit->setStyleSheet(QStringLiteral(
        "QLineEdit{background:#26262A;border:1px solid #3A3A42;border-radius:8px;padding:6px 10px;}"));
    connect(m_hotkeyEdit, &QKeySequenceEdit::editingFinished,
            this, &SettingsDialog::onHotkeyEdited);
    comboRow->addWidget(m_hotkeyEdit, 1);

    auto *resetBtn = new QPushButton(tr("Reset"), this);
    connect(resetBtn, &QPushButton::clicked, this, [this] {
        m_comboRadio->setChecked(true);
        m_hotkeyEdit->setKeySequence(GlobalHotkey::defaultSequence());
        onHotkeyEdited();
    });
    comboRow->addWidget(resetBtn);
    layout->addLayout(comboRow);

    auto *tapRow = new QHBoxLayout;
    tapRow->setSpacing(10);
    tapRow->addWidget(m_tapRadio);

    m_modCombo = new QComboBox(this);
    m_modCombo->addItem(GlobalHotkey::modKeyLabel(GlobalHotkey::ModKey::RightCmd),
                        int(GlobalHotkey::ModKey::RightCmd));
    m_modCombo->addItem(GlobalHotkey::modKeyLabel(GlobalHotkey::ModKey::RightAlt),
                        int(GlobalHotkey::ModKey::RightAlt));
    m_modCombo->addItem(GlobalHotkey::modKeyLabel(GlobalHotkey::ModKey::RightShift),
                        int(GlobalHotkey::ModKey::RightShift));
    m_modCombo->addItem(GlobalHotkey::modKeyLabel(GlobalHotkey::ModKey::RightCtrl),
                        int(GlobalHotkey::ModKey::RightCtrl));
    m_modCombo->setCurrentIndex(m_modCombo->findData(int(m_hotkey->modifierKey())));
    tapRow->addWidget(m_modCombo, 1);
    layout->addLayout(tapRow);

    auto applyTapChoice = [this] {
        if (!m_tapRadio->isChecked())
            return;
        const auto key = GlobalHotkey::ModKey(m_modCombo->currentData().toInt());
        // Save the choice either way; whether it can register right now is a
        // separate question from what the user asked for.
        QSettings s;
        s.setValue(QStringLiteral("hotkeyMode"), QStringLiteral("tap"));
        s.setValue(QStringLiteral("modTapKey"), int(key));

        if (m_hotkey->setModifierTap(key)) {
            m_hotkeyStatus->setText(tr("Tap %1 on its own to start and stop. Active when "
                                       "you close Settings.")
                                        .arg(m_hotkey->comboLabel()));
        } else if (m_hotkey->needsAccessibility()) {
            // Silently doing nothing here is what made the shortcut look
            // broken: the mode was selected but never registered.
            m_hotkeyStatus->setText(tr("Tap %1 needs Accessibility permission. Turn "
                                       "Whisperlet on in Privacy & Security, then it "
                                       "starts working on its own.")
                                        .arg(GlobalHotkey::modKeyLabel(key)));
            TextInjector::requestPermission();
            TextInjector::openPermissionSettings();
        } else {
            m_hotkeyStatus->setText(tr("Could not register that key."));
        }
    };
    connect(m_tapRadio, &QRadioButton::toggled, this, [applyTapChoice](bool on) {
        if (on) applyTapChoice();
    });
    connect(m_modCombo, &QComboBox::currentIndexChanged, this,
            [applyTapChoice](int) { applyTapChoice(); });
    connect(m_comboRadio, &QRadioButton::toggled, this, [this](bool on) {
        if (on) onHotkeyEdited();
    });

    m_hotkeyStatus = new QLabel(this);
    m_hotkeyStatus->setObjectName(QStringLiteral("cardMeta"));
    m_hotkeyStatus->setWordWrap(true);
    m_hotkeyStatus->setText(tr("Click the field, then press the keys. Needs at least one "
                               "modifier (Ctrl/Cmd/Alt/Shift). Works while the app is in "
                               "the background."));
    layout->addWidget(m_hotkeyStatus);

    // --- dictation ----------------------------------------------------------
    auto *pasteBox = new QCheckBox(tr("Paste dictated text into the active app"), this);
    pasteBox->setChecked(QSettings().value(QStringLiteral("pasteAfterDictation"), true).toBool());
    pasteBox->setToolTip(tr("When recording is started with the global shortcut while "
                            "another app is focused, the transcript is pasted there. "
                            "On macOS this needs the Accessibility permission."));
    connect(pasteBox, &QCheckBox::toggled, this, [](bool on) {
        QSettings().setValue(QStringLiteral("pasteAfterDictation"), on);
    });
    layout->addWidget(pasteBox);

#ifdef Q_OS_MAC
    // Only shown while the permission is missing — once granted there's
    // nothing to act on, so the row disappears instead of nagging.
    if (!TextInjector::canInject()) {
        auto *permRow = new QHBoxLayout;
        permRow->setSpacing(10);

        auto *warn = new QLabel(tr("Accessibility access is off, so dictated text is "
                                   "copied to the clipboard instead of typed."), this);
        warn->setObjectName(QStringLiteral("cardMeta"));
        warn->setWordWrap(true);
        permRow->addWidget(warn, 1);

        auto *openBtn = new QPushButton(tr("Open Settings"), this);
        connect(openBtn, &QPushButton::clicked, this, [] {
            TextInjector::requestPermission(); // adds us to the list
            TextInjector::openPermissionSettings();
        });
        permRow->addWidget(openBtn);
        layout->addLayout(permRow);
    }
#endif

    auto *keepAudioBox = new QCheckBox(tr("Keep recordings after transcribing"), this);
    keepAudioBox->setChecked(QSettings().value(QStringLiteral("keepAudio"), false).toBool());
    keepAudioBox->setToolTip(tr("Off by default: the audio is deleted once the transcript "
                                "exists. Turn on to replay a recording or run it again "
                                "through a different model — clips are a few MB per minute."));
    connect(keepAudioBox, &QCheckBox::toggled, this, [](bool on) {
        QSettings().setValue(QStringLiteral("keepAudio"), on);
    });
    layout->addWidget(keepAudioBox);

    layout->addStretch(1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(m_models, &ModelManager::downloadProgress, this, &SettingsDialog::onProgress);
    connect(m_models, &ModelManager::downloadFinished, this, &SettingsDialog::onFinished);

    for (const ModelInfo &info : ModelCatalog::all())
        refreshRow(info.id);
}

void SettingsDialog::refreshRow(const QString &id)
{
    auto it = m_rows.find(id);
    if (it == m_rows.end())
        return;
    Row &r = *it;

    const bool downloaded = m_models->isDownloaded(id);
    const bool downloading = m_models->isDownloading(id);

    r.active->setEnabled(downloaded);
    // Block the toggled->setActiveModelId round-trip while reflecting state.
    r.active->blockSignals(true);
    r.active->setChecked(downloaded && m_models->activeModelId() == id);
    r.active->blockSignals(false);

    r.progress->setVisible(downloading);
    if (!downloading)
        r.progress->setValue(0);

    if (downloading)
        r.action->setText(tr("Cancel"));
    else if (downloaded)
        r.action->setText(tr("Delete"));
    else
        r.action->setText(tr("Download"));
}

void SettingsDialog::onHotkeyEdited()
{
    if (!m_comboRadio->isChecked())
        return;

    const QKeySequence seq = m_hotkeyEdit->keySequence();

    // Empty (user hit the clear button) — keep the current one.
    if (seq.isEmpty()) {
        m_hotkeyEdit->setKeySequence(m_hotkey->sequence());
        return;
    }
    if (seq == m_hotkey->sequence())
        return;

    // A global hotkey without modifiers would swallow plain typing systemwide.
    if (seq[0].keyboardModifiers() == Qt::NoModifier) {
        m_hotkeyStatus->setText(tr("⚠ Needs at least one modifier key (Ctrl/Cmd/Alt/Shift)."));
        m_hotkeyEdit->setKeySequence(m_hotkey->sequence());
        return;
    }

    if (m_hotkey->setSequence(seq)) {
        QSettings s;
        s.setValue(QStringLiteral("hotkeyMode"), QStringLiteral("combo"));
        s.setValue(QStringLiteral("globalHotkey"), seq.toString(QKeySequence::PortableText));
        m_hotkeyStatus->setText(tr("Shortcut set to %1. Active when you close Settings.")
                                    .arg(m_hotkey->comboLabel()));
    } else {
        m_hotkeyStatus->setText(tr("⚠ That key can't be used as a global shortcut. Kept %1.")
                                    .arg(m_hotkey->comboLabel()));
        m_hotkeyEdit->setKeySequence(m_hotkey->sequence());
    }
}

void SettingsDialog::onDownloadClicked(const QString &id)
{
    m_models->download(id);
    refreshRow(id);
}

void SettingsDialog::onDeleteClicked(const QString &id)
{
    if (m_models->activeModelId() == id) {
        QMessageBox::information(this, tr("Model in use"),
                                 tr("This model is currently selected. "
                                    "Pick another model before deleting it."));
        return;
    }
    m_models->removeDownloaded(id);
    refreshRow(id);
}

void SettingsDialog::onProgress(const QString &id, qint64 received, qint64 total)
{
    auto it = m_rows.find(id);
    if (it == m_rows.end())
        return;

    if (total <= 0) {
        if (const ModelInfo *info = ModelCatalog::find(id))
            total = info->approxBytes; // server didn't say — use catalog estimate
    }
    if (total > 0)
        it->progress->setValue(static_cast<int>(received * 100 / total));
}

void SettingsDialog::onFinished(const QString &id, bool ok, const QString &error)
{
    refreshRow(id);
    if (!ok && !error.contains(QStringLiteral("canceled"), Qt::CaseInsensitive)) {
        QMessageBox::warning(this, tr("Download failed"),
                             tr("Could not download the model:\n%1").arg(error));
    }
}
