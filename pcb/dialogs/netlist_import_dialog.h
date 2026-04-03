#ifndef NETLIST_IMPORT_DIALOG_H
#define NETLIST_IMPORT_DIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QGroupBox>
#include "../import/netlist_importer.h"

/**
 * @brief Wizard dialog for importing netlists into the PCB editor.
 * 
 * Steps:
 * 1. Source Selection - Choose file or pull from open schematic
 * 2. Review Components - View components, assign footprints, auto-suggest
 * 3. Review Nets - View net connectivity
 * 4. Validation & Import - Check for errors, apply to PCB
 */
class NetlistImportDialog : public QDialog {
    Q_OBJECT

public:
    explicit NetlistImportDialog(QWidget* parent = nullptr);
    ~NetlistImportDialog();

    /**
     * @brief Get the final ECOPackage ready for PCB application.
     * Only valid after successful import.
     */
    ECOPackage resultECOPackage() const;

signals:
    void importRequested(const ECOPackage& pkg);

private slots:
    void onNext();
    void onBack();
    void onBrowseFile();
    void onSourceChanged(int index);
    void onAutoSuggestFootprints();
    void onFootprintCellChanged(int row, int col);
    void onImport();
    void updateButtonStates();
    void updateValidationStatus();

private:
    void setupUI();
    QWidget* createStepSource();
    QWidget* createStepComponents();
    QWidget* createStepNets();
    QWidget* createStepValidation();
    void populateComponentTable();
    void populateNetTable();
    void loadNetlistFile(const QString& filePath);
    void loadFromSchematic();
    QStringList getAvailableFootprints();
    void applyTheme();

    QStackedWidget* m_stackedWidget;
    QLabel* m_stepLabel;
    QPushButton* m_backBtn;
    QPushButton* m_nextBtn;
    QPushButton* m_cancelBtn;

    // Step 1: Source
    QComboBox* m_sourceCombo;
    QLineEdit* m_filePathEdit;
    QPushButton* m_browseBtn;
    QComboBox* m_formatCombo;
    QLabel* m_sourceSummaryLabel;

    // Step 2: Components
    QTableWidget* m_componentTable;
    QPushButton* m_autoSuggestBtn;
    QLabel* m_componentSummaryLabel;

    // Step 3: Nets
    QTableWidget* m_netTable;
    QLabel* m_netSummaryLabel;

    // Step 4: Validation
    QTextEdit* m_validationText;
    QPushButton* m_importBtn;
    QLabel* m_statusLabel;

    int m_currentStep = 0;
    int m_totalSteps = 4;

    NetlistImportPackage m_importPkg;
    ECOPackage m_resultECO;
    bool m_importReady = false;
};

#endif // NETLIST_IMPORT_DIALOG_H
