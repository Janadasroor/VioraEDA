#ifndef GERBER_EXPORT_DIALOG_H
#define GERBER_EXPORT_DIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include "../layers/pcb_layer.h"

class GerberExportDialog : public QDialog {
    Q_OBJECT

public:
    enum class Mode {
        Gerber,
        Pdf
    };

    explicit GerberExportDialog(Mode mode = Mode::Gerber, QWidget* parent = nullptr);
    ~GerberExportDialog();

    QList<int> selectedLayers() const;
    QString outputDirectory() const;
    bool generateDrillFile() const;
    bool exportCombinedPdf() const;
    bool pdfPlotOneToOne() const;
    bool pdfBlackAndWhite() const;
    bool pdfOpenAfterExport() const;

private slots:
    void onBrowse();
    void onExport();
    void onFilterTextChanged(const QString& text);

private:
    void setupUI();
    void populateLayers();
    void refreshLayerVisibility();

    QListWidget* m_layerList;
    QLineEdit* m_dirEdit;
    QLineEdit* m_filterEdit;
    QCheckBox* m_drillCheck;
    QCheckBox* m_combinedPdfCheck = nullptr;
    QCheckBox* m_pdfBlackAndWhiteCheck = nullptr;
    QCheckBox* m_pdfOpenAfterExportCheck = nullptr;
    QComboBox* m_pdfScaleMode = nullptr;
    Mode m_mode = Mode::Gerber;
};

#endif // GERBER_EXPORT_DIALOG_H
