#include "mos_circuit_architect.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QHeaderView>
#include <QLabel>
#include <QCheckBox>
#include <QInputDialog>
#include <QLineEdit>
#include "../schematic/dialogs/waveform_draw_widget.h"
#include "../schematic/dialogs/waveform_engine.h"
#include "../simulator/bridge/sim_manager.h"
#include "../simulator/synthesis/bv_mos_synthesizer.h"
#include "../simulator/synthesis/ideal_switch_synthesizer.h"
#include <QGroupBox>
#include <QSplitter>

MosCircuitArchitect::MosCircuitArchitect(QWidget *parent)
    : QDialog(parent), m_currentSynthesizer(nullptr) {
    setWindowTitle("MOS Circuit Architect - Custom Waveform Native Synthesis");
    setMinimumSize(900, 600);
    
    // Register topologies
    m_synthesizers.append(new BVMosSynthesizer());
    m_synthesizers.append(new IdealSwitchSynthesizer());
    m_currentSynthesizer = m_synthesizers.first();

    setupUi();
    loadTopologies();
}

MosCircuitArchitect::~MosCircuitArchitect() {
    qDeleteAll(m_synthesizers);
}

void MosCircuitArchitect::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);

    auto* splitter = new QSplitter(Qt::Horizontal);
    
    // --- Left Panel ---
    auto* leftPanel = new QWidget;
    auto* leftLayout = new QVBoxLayout(leftPanel);
    
    auto* drawLabel = new QLabel("Draw Custom Target Waveform:");
    drawLabel->setStyleSheet("font-weight: bold; color: #a1a1aa;");
    leftLayout->addWidget(drawLabel);
    
    m_drawWidget = new WaveformDrawWidget(this);
    m_drawWidget->setMinimumSize(400, 300);
    m_drawWidget->setSnapToGrid(false);
    leftLayout->addWidget(m_drawWidget, 1);
    
    auto* controlsLayout = new QHBoxLayout;
    auto* clearBtn = new QPushButton("Clear Canvas");
    auto* snapCheck = new QCheckBox("Snap to Grid");
    auto* stepCheck = new QCheckBox("Step Mode");
    controlsLayout->addWidget(clearBtn);
    controlsLayout->addStretch();
    controlsLayout->addWidget(snapCheck);
    controlsLayout->addWidget(stepCheck);
    leftLayout->addLayout(controlsLayout);
    
    connect(clearBtn, &QPushButton::clicked, m_drawWidget, &WaveformDrawWidget::clearPoints);
    connect(snapCheck, &QCheckBox::toggled, m_drawWidget, &WaveformDrawWidget::setSnapToGrid);
    connect(stepCheck, &QCheckBox::toggled, m_drawWidget, &WaveformDrawWidget::setStepMode);

    // --- Advanced Tools ---
    auto* advGroup = new QGroupBox("Waveform Options");
    auto* advLayout = new QGridLayout(advGroup);
    
    auto* polyCheck = new QCheckBox("Polyline Mode");
    auto* revBtn = new QPushButton("Reverse Time");
    auto* squareBtn = new QPushButton("Gen Square");
    auto* bitBtn = new QPushButton("Gen Bitstream");
    auto* scaleUpBtn = new QPushButton("Scale (x1.1)");
    auto* scaleDownBtn = new QPushButton("Scale (x0.9)");
    
    advLayout->addWidget(polyCheck, 0, 0);
    advLayout->addWidget(revBtn, 0, 1);
    advLayout->addWidget(squareBtn, 1, 0);
    advLayout->addWidget(bitBtn, 1, 1);
    advLayout->addWidget(scaleUpBtn, 2, 0);
    advLayout->addWidget(scaleDownBtn, 2, 1);
    leftLayout->addWidget(advGroup);

    connect(polyCheck, &QCheckBox::toggled, m_drawWidget, &WaveformDrawWidget::setPolylineMode);
    connect(revBtn, &QPushButton::clicked, m_drawWidget, &WaveformDrawWidget::reverseTime);
    connect(squareBtn, &QPushButton::clicked, this, &MosCircuitArchitect::onSquareClicked);
    connect(bitBtn, &QPushButton::clicked, this, &MosCircuitArchitect::onBitstreamClicked);
    connect(scaleUpBtn, &QPushButton::clicked, [this](){ m_drawWidget->scaleValue(1.1); });
    connect(scaleDownBtn, &QPushButton::clicked, [this](){ m_drawWidget->scaleValue(0.9); });
    connect(m_drawWidget, &WaveformDrawWidget::pointsChanged, this, &MosCircuitArchitect::updatePreview);
    
    // --- Right Panel ---
    auto* rightPanel = new QWidget;
    auto* rightLayout = new QVBoxLayout(rightPanel);
    
    auto* formLayout = new QFormLayout;
    m_topologyCombo = new QComboBox;
    formLayout->addRow("Topology:", m_topologyCombo);
    rightLayout->addLayout(formLayout);
    
    auto* paramLabel = new QLabel("Circuit Parameters:");
    paramLabel->setStyleSheet("font-weight: bold; color: #a1a1aa;");
    rightLayout->addWidget(paramLabel);
    
    m_paramTable = new QTableWidget(0, 3);
    m_paramTable->setHorizontalHeaderLabels({"Parameter", "Value", "Unit"});
    m_paramTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_paramTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_paramTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    rightLayout->addWidget(m_paramTable, 1);
    
    auto* previewLabel = new QLabel("SPICE Subcircuit Netlist Preview:");
    previewLabel->setStyleSheet("font-weight: bold; color: #a1a1aa;");
    rightLayout->addWidget(previewLabel);
    
    m_previewArea = new QTextEdit;
    m_previewArea->setReadOnly(true);
    m_previewArea->setStyleSheet("background-color: #0d0d0f; color: #10b981; font-family: monospace;");
    rightLayout->addWidget(m_previewArea, 2);
    
    auto* btnLayout = new QHBoxLayout;
    m_generateBtn = new QPushButton("Generate Netlist");
    m_generateBtn->setStyleSheet("background-color: #27272a; padding: 6px 12px;");
    m_verifyBtn = new QPushButton("Run Verification in Simulator");
    m_verifyBtn->setStyleSheet("background-color: #007acc; color: white; font-weight: bold; padding: 6px 12px;");
    
    btnLayout->addWidget(m_generateBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_verifyBtn);
    rightLayout->addLayout(btnLayout);
    
    splitter->addWidget(leftPanel);
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    mainLayout->addWidget(splitter);
    
    // Connects
    connect(m_topologyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MosCircuitArchitect::onTopologyChanged);
    connect(m_generateBtn, &QPushButton::clicked, this, &MosCircuitArchitect::onGenerateClicked);
    connect(m_verifyBtn, &QPushButton::clicked, this, &MosCircuitArchitect::onVerifyClicked);
    connect(m_paramTable, &QTableWidget::itemChanged, this, &MosCircuitArchitect::onParameterChanged);
}

void MosCircuitArchitect::loadTopologies() {
    m_topologyCombo->blockSignals(true);
    m_topologyCombo->clear();
    for (const auto* synth : m_synthesizers) {
        m_topologyCombo->addItem(synth->getTopologyName());
    }
    m_topologyCombo->blockSignals(false);
    
    if (!m_synthesizers.isEmpty()) {
        onTopologyChanged(0);
    }
}

void MosCircuitArchitect::onTopologyChanged(int index) {
    if (index < 0 || index >= m_synthesizers.size()) return;
    m_currentSynthesizer = m_synthesizers[index];
    populateParameters();
    updatePreview();
}

void MosCircuitArchitect::populateParameters() {
    m_paramTable->blockSignals(true);
    auto params = m_currentSynthesizer->getRequiredParameters();
    m_paramTable->setRowCount(params.size());
    
    for (int i = 0; i < params.size(); ++i) {
        auto* nameItem = new QTableWidgetItem(params[i].name);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        nameItem->setToolTip(params[i].description);
        m_paramTable->setItem(i, 0, nameItem);
        
        auto* valItem = new QTableWidgetItem(params[i].defaultValue);
        m_paramTable->setItem(i, 1, valItem);
        
        auto* uItem = new QTableWidgetItem(params[i].unit);
        uItem->setFlags(uItem->flags() & ~Qt::ItemIsEditable);
        m_paramTable->setItem(i, 2, uItem);
    }
    m_paramTable->blockSignals(false);
}

void MosCircuitArchitect::onParameterChanged() {
    // Only manual updates via the Generate button for now, to save performance when actively typing
}

void MosCircuitArchitect::updatePreview() {
    if (!m_currentSynthesizer) return;
    
    QVector<QPointF> pts = m_drawWidget->points();
    if (pts.isEmpty()) {
        m_previewArea->setPlainText("No points drawn. Draw a waveform to generate the netlist.");
        return;
    }
    
    QMap<QString, QString> pMap;
    for (int i = 0; i < m_paramTable->rowCount(); ++i) {
        QString key = m_paramTable->item(i, 0)->text();
        QString val = m_paramTable->item(i, 1)->text();
        pMap.insert(key, val);
    }
    
    QString netlist = m_currentSynthesizer->synthesizeNetlist("CUSTOM_WAVE_GEN", pts, pMap);
    m_previewArea->setPlainText(netlist);
}

void MosCircuitArchitect::onGenerateClicked() {
    updatePreview();
}

void MosCircuitArchitect::onVerifyClicked() {
    updatePreview();
    QString netlist = m_previewArea->toPlainText();
    if (netlist.isEmpty() || netlist.contains("No points drawn")) {
        QMessageBox::warning(this, "Empty Model", "Cannot verify an empty model. Please draw a waveform.");
        return;
    }
    
    // Construct automated testbench wrapper
    QString tb;
    tb += "* Verification Testbench for MosCircuitArchitect\n";
    tb += netlist + "\n";
    
    QString vddVal = "5V";
    if (m_paramTable->rowCount() > 0 && m_paramTable->item(0, 0)->text() == "VDD") {
        vddVal = m_paramTable->item(0, 1)->text();
    }

    tb += "Vdd vdd_node 0 " + vddVal + "\n";
    tb += "X_target out_node vdd_node 0 CUSTOM_WAVE_GEN\n";
    tb += "Rload out_node 0 1Meg\n";
    
    // Determine transient timing based on drawn points x-axis max
    double tMax = 1e-3;
    QVector<QPointF> pts = m_drawWidget->points();
    if (!pts.isEmpty()) {
        tMax = pts.last().x() * 1.2; // 20% margin
        if (tMax <= 0) tMax = 1e-3;
    }
    double tStep = tMax / 1000.0;
    
    tb += QString(".tran %1 %2\n").arg(tStep, 0, 'g', 4).arg(tMax, 0, 'g', 4);
    tb += ".end\n";
    
    // Trigger global simulation. Main app listens to this and spawns WaveformViewer.
    SimManager::instance().runNetlistText(tb);
    QMessageBox::information(this, "Verification Started", "The testbench netlist has been submitted to the SPICE engine.\nOnce completed, the raw results will open.");
}

void MosCircuitArchitect::onSquareClicked() {
    bool ok;
    double duty = QInputDialog::getDouble(this, "Square Wave", "Duty Cycle (0.1 - 0.9):", 0.5, 0.1, 0.9, 2, &ok);
    if (!ok) return;

    auto pts = WaveformEngine::generateSquare(duty);
    m_drawWidget->setPoints(pts);
    updatePreview();
}

void MosCircuitArchitect::onBitstreamClicked() {
    bool ok;
    QString bits = QInputDialog::getText(this, "Bitstream", "Enter Bits (e.g. 10110):", QLineEdit::Normal, "101010", &ok);
    if (!ok || bits.isEmpty()) return;

    auto pts = WaveformEngine::generateBitstream(bits);
    m_drawWidget->setPoints(pts);
    updatePreview();
}
