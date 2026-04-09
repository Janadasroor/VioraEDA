#include "gerber_export_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QMessageBox>
#include <QStandardPaths>
#include "../gerber/gerber_exporter.h"

GerberExportDialog::GerberExportDialog(Mode mode, QWidget* parent)
    : QDialog(parent)
    , m_mode(mode) {
    setupUI();
    populateLayers();
}

GerberExportDialog::~GerberExportDialog() {}

void GerberExportDialog::setupUI() {
    setWindowTitle(m_mode == Mode::Pdf ? "Export PDF Plots" : "Generate Gerber Files");
    resize(500, 600);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);

    // 1. Output Directory
    QGroupBox* dirGroup = new QGroupBox("Output Settings");
    QHBoxLayout* dirLayout = new QHBoxLayout(dirGroup);
    m_dirEdit = new QLineEdit();
    m_dirEdit->setText(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
                       (m_mode == Mode::Pdf ? "/PDF_Plots" : "/Gerbers"));
    QPushButton* browseBtn = new QPushButton("Browse...");
    connect(browseBtn, &QPushButton::clicked, this, &GerberExportDialog::onBrowse);
    dirLayout->addWidget(m_dirEdit);
    dirLayout->addWidget(browseBtn);
    mainLayout->addWidget(dirGroup);

    // 2. Layer Selection
    QGroupBox* layerGroup = new QGroupBox("Select Layers to Plot");
    QVBoxLayout* layerLayout = new QVBoxLayout(layerGroup);
    m_filterEdit = new QLineEdit();
    m_filterEdit->setPlaceholderText("Filter layers...");
    connect(m_filterEdit, &QLineEdit::textChanged, this, &GerberExportDialog::onFilterTextChanged);
    layerLayout->addWidget(m_filterEdit);

    m_layerList = new QListWidget();
    m_layerList->setSelectionMode(QAbstractItemView::NoSelection);
    layerLayout->addWidget(m_layerList);
    mainLayout->addWidget(layerGroup);

    // 3. Drill Options
    m_drillCheck = new QCheckBox("Generate Excellon Drill File (.drl)");
    m_drillCheck->setChecked(true);
    if (m_mode == Mode::Gerber) {
        mainLayout->addWidget(m_drillCheck);
    }
    if (m_mode == Mode::Pdf) {
        QGroupBox* pdfGroup = new QGroupBox("PDF Plot Options");
        QVBoxLayout* pdfLayout = new QVBoxLayout(pdfGroup);
        QHBoxLayout* scaleLayout = new QHBoxLayout();
        QLabel* scaleLabel = new QLabel("Scale:");
        m_pdfScaleMode = new QComboBox();
        m_pdfScaleMode->addItem("Fit To Page", "fit");
        m_pdfScaleMode->addItem("1:1", "1:1");
        scaleLayout->addWidget(scaleLabel);
        scaleLayout->addWidget(m_pdfScaleMode, 1);
        pdfLayout->addLayout(scaleLayout);
        m_pdfBlackAndWhiteCheck = new QCheckBox("Black and white");
        m_pdfBlackAndWhiteCheck->setChecked(false);
        pdfLayout->addWidget(m_pdfBlackAndWhiteCheck);
        mainLayout->addWidget(pdfGroup);

        m_combinedPdfCheck = new QCheckBox("Also export one combined board PDF");
        m_combinedPdfCheck->setChecked(true);
        mainLayout->addWidget(m_combinedPdfCheck);

        m_pdfOpenAfterExportCheck = new QCheckBox("Open exported PDF in viewer");
        m_pdfOpenAfterExportCheck->setChecked(true);
        mainLayout->addWidget(m_pdfOpenAfterExportCheck);
    }

    // 4. Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* exportBtn = new QPushButton(m_mode == Mode::Pdf ? "Export PDFs" : "Generate Files");
    QPushButton* cancelBtn = new QPushButton("Cancel");
    exportBtn->setStyleSheet("background-color: #007acc; color: white; font-weight: bold; padding: 8px;");
    
    connect(exportBtn, &QPushButton::clicked, this, &GerberExportDialog::onExport);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    
    btnLayout->addStretch();
    btnLayout->addWidget(exportBtn);
    btnLayout->addWidget(cancelBtn);
    mainLayout->addLayout(btnLayout);
}

void GerberExportDialog::populateLayers() {
    for (const auto& layer : PCBLayerManager::instance().layers()) {
        if (layer.type() == PCBLayer::Copper || layer.type() == PCBLayer::Silkscreen ||
            layer.type() == PCBLayer::Soldermask || layer.type() == PCBLayer::Paste ||
            layer.type() == PCBLayer::Courtyard || layer.type() == PCBLayer::Fabrication ||
            layer.type() == PCBLayer::EdgeCuts) {
            QListWidgetItem* item = new QListWidgetItem(layer.name());
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
            item->setData(Qt::UserRole, layer.id());
            item->setCheckState(Qt::Checked);
            m_layerList->addItem(item);
        }
    }
    refreshLayerVisibility();
}

void GerberExportDialog::onBrowse() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Output Directory", m_dirEdit->text());
    if (!dir.isEmpty()) m_dirEdit->setText(dir);
}

void GerberExportDialog::onExport() {
    QDir dir(m_dirEdit->text());
    if (!dir.exists()) dir.mkpath(".");

    accept();
}

QList<int> GerberExportDialog::selectedLayers() const {
    QList<int> ids;
    for (int i = 0; i < m_layerList->count(); ++i) {
        QListWidgetItem* item = m_layerList->item(i);
        if (item && item->checkState() == Qt::Checked) {
            ids.append(item->data(Qt::UserRole).toInt());
        }
    }
    return ids;
}

QString GerberExportDialog::outputDirectory() const {
    return m_dirEdit->text();
}

bool GerberExportDialog::generateDrillFile() const {
    return m_drillCheck && m_drillCheck->isChecked();
}

bool GerberExportDialog::exportCombinedPdf() const {
    return m_combinedPdfCheck && m_combinedPdfCheck->isChecked();
}

bool GerberExportDialog::pdfPlotOneToOne() const {
    return m_pdfScaleMode && m_pdfScaleMode->currentData().toString() == "1:1";
}

bool GerberExportDialog::pdfBlackAndWhite() const {
    return m_pdfBlackAndWhiteCheck && m_pdfBlackAndWhiteCheck->isChecked();
}

bool GerberExportDialog::pdfOpenAfterExport() const {
    return m_pdfOpenAfterExportCheck && m_pdfOpenAfterExportCheck->isChecked();
}

void GerberExportDialog::onFilterTextChanged(const QString& text) {
    Q_UNUSED(text);
    refreshLayerVisibility();
}

void GerberExportDialog::refreshLayerVisibility() {
    const QString filter = m_filterEdit ? m_filterEdit->text().trimmed() : QString();
    for (int i = 0; i < m_layerList->count(); ++i) {
        QListWidgetItem* item = m_layerList->item(i);
        if (!item) {
            continue;
        }
        const bool matches = filter.isEmpty() || item->text().contains(filter, Qt::CaseInsensitive);
        item->setHidden(!matches);
    }
}
