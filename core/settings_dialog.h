#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include <QDialog>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QListWidget>
#include <QStackedWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include "ui/color_button.h"

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog();

private Q_SLOTS:
    void onAccept();

private:
    void setupUI();
    void loadSettings();

    // General
    QCheckBox* m_autoSaveCheck;
    QSpinBox* m_autoSaveSpin;
    QComboBox* m_themeCombo;
    QCheckBox* m_snapGridCheck;
    QCheckBox* m_autoFocusCrossProbeCheck;
    QCheckBox* m_realtimeWireUpdateCheck;
    ColorButton* m_wireColorBtn = nullptr;

    // Simulator
    QComboBox* m_solverCombo;
    QComboBox* m_integrationCombo;
    QDoubleSpinBox* m_reltolSpin;
    QDoubleSpinBox* m_abstolSpin;
    QDoubleSpinBox* m_vntolSpin;
    QDoubleSpinBox* m_gminSpin;
    QSpinBox* m_maxIterSpin;
    QCheckBox* m_showFullSimulationPanelCheck;

    // PCB
    QCheckBox* m_enablePcbEditorsCheck;
    
    // AI
    QLineEdit* m_geminiKeyEdit;
    QComboBox* m_geminiOverlayModelCombo;
    QComboBox* m_geminiChatModelCombo;
    QPushButton* m_refreshModelsBtn;
    QCheckBox* m_aiOverlayCheck;
    QCheckBox* m_aiChatCheck;
    QCheckBox* m_aiErcCheck;
    
    // Libraries
    QTextEdit* m_symbolPathsEdit;
    QTextEdit* m_modelPathsEdit;
    QTextEdit* m_libraryRootsEdit;
    QCheckBox* m_kicadDisabledCheck;
    QCheckBox* m_kicadBasicsOnlyCheck;

    QListWidget* m_navMenu;
    QStackedWidget* m_pagesStack;
};

#endif // SETTINGS_DIALOG_H
