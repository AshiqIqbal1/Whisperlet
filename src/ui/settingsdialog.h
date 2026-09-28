#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QMap>

class GlobalHotkey;
class ModelManager;
class UpdateChecker;
class QComboBox;
class QKeySequenceEdit;
class QLabel;
class QProgressBar;
class QPushButton;
class QRadioButton;

// Settings — model management + the global shortcut.
// One row per catalog model:
//   [radio: active] Name        size      [Download|Cancel|Delete] [progress]
// The radio is only enabled once that model's file is on disk.
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(ModelManager *models, GlobalHotkey *hotkey,
                            QWidget *parent = nullptr);

private slots:
    void onDownloadClicked(const QString &id);
    void onDeleteClicked(const QString &id);
    void onProgress(const QString &id, qint64 received, qint64 total);
    void onFinished(const QString &id, bool ok, const QString &error);
    void onHotkeyEdited();
    void onCheckForUpdatesClicked();

private:
    struct Row
    {
        QRadioButton *active = nullptr;
        QLabel *size = nullptr;
        QPushButton *action = nullptr;
        QProgressBar *progress = nullptr;
    };

    void refreshRow(const QString &id);

    ModelManager *m_models = nullptr;
    GlobalHotkey *m_hotkey = nullptr;
    QKeySequenceEdit *m_hotkeyEdit = nullptr;
    QRadioButton *m_comboRadio = nullptr;
    QRadioButton *m_tapRadio = nullptr;
    QComboBox *m_modCombo = nullptr;
    QLabel *m_hotkeyStatus = nullptr;
    bool m_openedAccessibilitySettings = false;
    QMap<QString, Row> m_rows;

    UpdateChecker *m_updateChecker = nullptr;
    QPushButton *m_updateButton = nullptr;
    QLabel *m_updateStatus = nullptr;
    QPushButton *m_openReleaseButton = nullptr;
    QString m_releaseUrl;
};

#endif // SETTINGSDIALOG_H
