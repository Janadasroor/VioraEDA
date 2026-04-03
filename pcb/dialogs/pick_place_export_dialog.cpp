#include "pick_place_export_dialog.h"
#include "../core/theme_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QGroupBox>
#include <QMessageBox>
#include <QFileInfo>
#include <QTextBrowser>
#include <QStandardPaths>

PickPlaceExportDialog::PickPlaceExportDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Export Pick and Place File");
    resize(520, 550);
    setupUI();
    updatePreview();
}

PickPlaceExportDialog::~PickPlaceExportDialog() = default;

ManufacturingExporter::PickPlaceOptions PickPlaceExportDialog::options() const {
    ManufacturingExporter::PickPlaceOptions opts;
    opts.format = (m_formatCombo->currentIndex() == 0)
                      ? ManufacturingExporter::CSV
                      : ManufacturingExporter::TSV;
    opts.includeTopSide = m_topSideCheck->isChecked();
    opts.includeBottomSide = m_bottomSideCheck->isChecked();
    opts.includeFiducials = m_includeFiducialsCheck->isChecked();
    opts.includeTestPoints = m_includeTPCheck->isChecked();
    opts.useMillimeters = m_metricRadio->isChecked();
    opts.includeValue = m_includeValueCheck->isChecked();
    opts.includeFootprint = m_includeFootprintCheck->isChecked();
    return opts;
}

void PickPlaceExportDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // === Output File ===
    QGroupBox* fileGroup = new QGroupBox("Output File");
    QHBoxLayout* fileLayout = new QHBoxLayout(fileGroup);
    m_filePathEdit = new QLineEdit();
    m_filePathEdit->setText(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/pick_place.csv");
    m_browseBtn = new QPushButton("Browse...");
    fileLayout->addWidget(m_filePathEdit);
    fileLayout->addWidget(m_browseBtn);
    connect(m_browseBtn, &QPushButton::clicked, this, &PickPlaceExportDialog::onBrowse);
    mainLayout->addWidget(fileGroup);

    // === Format Options ===
    QGroupBox* fmtGroup = new QGroupBox("Format Options");
    QGridLayout* fmtLayout = new QGridLayout(fmtGroup);
    fmtLayout->addWidget(new QLabel("Separator:"), 0, 0);
    m_formatCombo = new QComboBox();
    m_formatCombo->addItem("CSV (Comma-separated)");
    m_formatCombo->addItem("TSV (Tab-separated / Excel)");
    fmtLayout->addWidget(m_formatCombo, 0, 1);

    fmtLayout->addWidget(new QLabel("Units:"), 1, 0);
    QWidget* unitWidget = new QWidget();
    QHBoxLayout* unitLayout = new QHBoxLayout(unitWidget);
    unitLayout->setContentsMargins(0, 0, 0, 0);
    m_metricRadio = new QRadioButton("Millimeters");
    mImperialRadio = new QRadioButton("Inches");
    m_metricRadio->setChecked(true);
    unitLayout->addWidget(m_metricRadio);
    unitLayout->addWidget(mImperialRadio);
    unitLayout->addStretch();
    fmtLayout->addWidget(unitWidget, 1, 1);
    mainLayout->addWidget(fmtGroup);

    // === Sides ===
    QGroupBox* sideGroup = new QGroupBox("Include Sides");
    QHBoxLayout* sideLayout = new QHBoxLayout(sideGroup);
    m_topSideCheck = new QCheckBox("Top Side");
    m_bottomSideCheck = new QCheckBox("Bottom Side");
    m_topSideCheck->setChecked(true);
    m_bottomSideCheck->setChecked(true);
    sideLayout->addWidget(m_topSideCheck);
    sideLayout->addWidget(m_bottomSideCheck);
    sideLayout->addStretch();
    mainLayout->addWidget(sideGroup);

    // === Additional Options ===
    QGroupBox* optGroup = new QGroupBox("Additional Options");
    QVBoxLayout* optLayout = new QVBoxLayout(optGroup);
    m_includeValueCheck = new QCheckBox("Include Value column");
    m_includeValueCheck->setChecked(true);
    m_includeFootprintCheck = new QCheckBox("Include Footprint column");
    m_includeFootprintCheck->setChecked(true);
    m_includeFiducialsCheck = new QCheckBox("Include fiducial markers (FID*)");
    m_includeTPCheck = new QCheckBox("Include test points (TP*)");
    optLayout->addWidget(m_includeValueCheck);
    optLayout->addWidget(m_includeFootprintCheck);
    optLayout->addWidget(m_includeFiducialsCheck);
    optLayout->addWidget(m_includeTPCheck);
    mainLayout->addWidget(optGroup);

    // === Preview ===
    m_previewLabel = new QLabel();
    m_previewLabel->setWordWrap(true);
    m_previewLabel->setStyleSheet(
        "background: palette(alternate-base); padding: 8px; border-radius: 4px;");
    mainLayout->addWidget(m_previewLabel);

    // === Buttons ===
    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_cancelBtn = new QPushButton("Cancel");
    m_exportBtn = new QPushButton("📤 Export");
    m_exportBtn->setObjectName("primaryButton");
    m_exportBtn->setStyleSheet(
        "background-color: #007acc; color: white; font-weight: bold; padding: 8px;");
    btnLayout->addStretch();
    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addWidget(m_exportBtn);
    mainLayout->addLayout(btnLayout);

    // Connections
    connect(m_exportBtn, &QPushButton::clicked, this, &PickPlaceExportDialog::onExport);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PickPlaceExportDialog::onFormatChanged);
    connect(m_metricRadio, &QRadioButton::toggled, this, &PickPlaceExportDialog::updatePreview);
    connect(mImperialRadio, &QRadioButton::toggled, this, &PickPlaceExportDialog::updatePreview);
    connect(m_includeValueCheck, &QCheckBox::toggled, this, &PickPlaceExportDialog::updatePreview);
    connect(m_includeFootprintCheck, &QCheckBox::toggled, this, &PickPlaceExportDialog::updatePreview);
    connect(m_topSideCheck, &QCheckBox::toggled, this, &PickPlaceExportDialog::updatePreview);
    connect(m_bottomSideCheck, &QCheckBox::toggled, this, &PickPlaceExportDialog::updatePreview);
}

void PickPlaceExportDialog::onBrowse() {
    QString filter = (m_formatCombo->currentIndex() == 0)
                         ? "CSV Files (*.csv);;All Files (*)"
                         : "TSV Files (*.txt *.tsv);;All Files (*)";
    QString path = QFileDialog::getSaveFileName(this, "Save Pick and Place File",
                                                 m_filePathEdit->text(), filter);
    if (!path.isEmpty()) {
        m_filePathEdit->setText(path);
    }
}

void PickPlaceExportDialog::onExport() {
    QString path = m_filePathEdit->text().trimmed();
    if (path.isEmpty()) {
        QMessageBox::warning(this, "No Output File", "Please specify an output file path.");
        return;
    }

    // Validation only - actual export is done by MainWindow with the scene
    accept();
}

void PickPlaceExportDialog::onFormatChanged() {
    // Update file extension
    QString path = m_filePathEdit->text();
    if (m_formatCombo->currentIndex() == 0) {
        // CSV
        if (path.endsWith(".txt", Qt::CaseInsensitive) || path.endsWith(".tsv", Qt::CaseInsensitive)) {
            path.chop(4);
            path += ".csv";
        }
    } else {
        // TSV
        if (path.endsWith(".csv", Qt::CaseInsensitive)) {
            path.chop(4);
            path += ".txt";
        }
    }
    m_filePathEdit->setText(path);
    updatePreview();
}

void PickPlaceExportDialog::updatePreview() {
    bool csv = (m_formatCombo->currentIndex() == 0);
    const char sep = csv ? ',' : '\t';
    const QString unit = m_metricRadio->isChecked() ? "mm" : "in";

    QString preview;
    preview += QString("<b>Preview:</b><br>");
    preview += QString("<tt>Designator%1Mid X%2Mid Y%3Layer%4Rotation")
                   .arg(sep).arg(sep).arg(sep).arg(sep);
    if (m_includeFootprintCheck->isChecked())
        preview += QString("%1Footprint").arg(sep);
    if (m_includeValueCheck->isChecked())
        preview += QString("%1Value").arg(sep);
    preview += QString("</tt><br><br>");
    preview += QString("<b>Units:</b> %1<br>").arg(unit);
    preview += QString("<b>Sides:</b> %1 %2<br>")
                   .arg(m_topSideCheck->isChecked() ? "Top" : "")
                   .arg(m_bottomSideCheck->isChecked() ? "Bottom" : "");

    m_previewLabel->setText(preview);
}
