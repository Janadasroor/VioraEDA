#ifndef PICK_PLACE_EXPORT_DIALOG_H
#define PICK_PLACE_EXPORT_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QPushButton>
#include <QLabel>
#include "../manufacturing/manufacturing_exporter.h"

/**
 * @brief Dialog for configuring and exporting Pick-and-Place (centroid) files.
 */
class PickPlaceExportDialog : public QDialog {
    Q_OBJECT

public:
    explicit PickPlaceExportDialog(QWidget* parent = nullptr);
    ~PickPlaceExportDialog();

    ManufacturingExporter::PickPlaceOptions options() const;
    QString outputPath() const { return m_filePathEdit->text(); }

private slots:
    void onBrowse();
    void onExport();
    void onFormatChanged();

private:
    void setupUI();

    QLineEdit* m_filePathEdit;
    QPushButton* m_browseBtn;
    QComboBox* m_formatCombo;
    QRadioButton* m_metricRadio;
    QRadioButton* mImperialRadio;
    QCheckBox* m_topSideCheck;
    QCheckBox* m_bottomSideCheck;
    QCheckBox* m_includeFiducialsCheck;
    QCheckBox* m_includeTPCheck;
    QCheckBox* m_includeValueCheck;
    QCheckBox* m_includeFootprintCheck;
    QLabel* m_previewLabel;
    QPushButton* m_exportBtn;
    QPushButton* m_cancelBtn;

    void updatePreview();
};

#endif // PICK_PLACE_EXPORT_DIALOG_H
