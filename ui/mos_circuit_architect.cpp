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
#include <QGroupBox>
#include <QSplitter>
#include <QDebug>
#include <QUndoStack>
#include <QApplication>

#include "../schematic/editor/schematic_view.h"
#include "../schematic/editor/schematic_editor.h"
#include "../schematic/editor/schematic_commands.h"
#include "../schematic/factories/schematic_item_factory.h"
#include "../schematic/items/voltage_source_item.h"
#include "../schematic/items/generic_component_item.h"
#include "../schematic/items/schematic_spice_directive_item.h"
#include "../schematic/dialogs/waveform_draw_widget.h"
#include "../schematic/dialogs/waveform_engine.h"
#include "../simulator/bridge/sim_manager.h"
#include "../simulator/core/sim_value_parser.h"
#include "../simulator/synthesis/bv_mos_synthesizer.h"
#include "../simulator/synthesis/ideal_switch_synthesizer.h"
#include "../simulator/synthesis/power_electronics_synthesizers.h"
#include "waveform_viewer.h"

using namespace Flux::Model;

MosCircuitArchitect::MosCircuitArchitect(QWidget *parent)
    : QMainWindow(parent), m_currentSynthesizer(nullptr), m_sourceItem(nullptr) {
    setWindowTitle("MOS Circuit Architect - Power Electronics Vision");
    setMinimumSize(900, 600);
    setAttribute(Qt::WA_DeleteOnClose);
    
    // Register topologies
    m_synthesizers.append(new HalfBridgeSynthesizer());
    m_synthesizers.append(new PushPullSynthesizer());
    m_synthesizers.append(new MatrixSynthesizer());
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
    auto* central = new QWidget;
    setCentralWidget(central);
    auto* mainLayout = new QVBoxLayout(central);

    auto* splitter = new QSplitter(Qt::Horizontal);
    
    // --- Left Panel ---
    auto* leftPanel = new QWidget;
    auto* leftLayout = new QVBoxLayout(leftPanel);
    
    auto* drawLabel = new QLabel("Drawn Target Waveform (0 to 1.0 Cycle):");
    drawLabel->setStyleSheet("font-weight: bold; color: #a1a1aa;");
    leftLayout->addWidget(drawLabel);
    
    m_drawWidget = new WaveformDrawWidget;
    m_drawWidget->setMinimumSize(400, 300);
    leftLayout->addWidget(m_drawWidget, 1);
    
    auto* controlsLayout = new QHBoxLayout;
    auto* clearBtn = new QPushButton("Clear Canvas");
    auto* snapCheck = new QCheckBox("Snap to Grid");
    m_stepCheck = new QCheckBox("Step Mode");
    controlsLayout->addWidget(clearBtn);
    controlsLayout->addStretch();
    controlsLayout->addWidget(snapCheck);
    controlsLayout->addWidget(m_stepCheck);
    leftLayout->addLayout(controlsLayout);
    
    connect(clearBtn, &QPushButton::clicked, m_drawWidget, &WaveformDrawWidget::clearPoints);
    connect(snapCheck, &QCheckBox::toggled, m_drawWidget, &WaveformDrawWidget::setSnapToGrid);
    connect(m_stepCheck, &QCheckBox::toggled, m_drawWidget, &WaveformDrawWidget::setStepMode);

    // --- Advanced Tools ---
    auto* advGroup = new QGroupBox("Waveform Options");
    auto* advLayout = new QGridLayout(advGroup);
    
    auto* polyCheck = new QCheckBox("Polyline Mode");
    auto* revBtn = new QPushButton("Reverse Time");
    auto* squareBtn = new QPushButton("Square Wave...");
    auto* bitBtn = new QPushButton("Bitstream...");
    auto* scaleUpBtn = new QPushButton("Value x1.1");
    auto* scaleDownBtn = new QPushButton("Value x0.9");
    
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
    
    auto* paramLabel = new QLabel("Circuit Parameters (Auto-Synced to .tran):");
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
    m_previewArea->setStyleSheet("background-color: #0d0d0f; color: #10b981; font-family: monospace; font-size: 11pt; border: 1px solid #3f3f46;");
    rightLayout->addWidget(m_previewArea, 2);
    
    auto* btnLayout = new QHBoxLayout;
    m_runBtn = new QPushButton("Apply & Run Simulation");
    m_runBtn->setStyleSheet("background-color: #10b981; color: white; font-weight: bold; padding: 10px 20px; font-size: 11pt;");
    
    btnLayout->addStretch();
    btnLayout->addWidget(m_runBtn);
    btnLayout->addStretch();
    rightLayout->addLayout(btnLayout);
    
    splitter->addWidget(leftPanel);
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    mainLayout->addWidget(splitter);
    
    // Connects
    connect(m_topologyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MosCircuitArchitect::onTopologyChanged);
    connect(m_runBtn, &QPushButton::clicked, this, &MosCircuitArchitect::onVerifyClicked);
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
    
    // Auto-sync PERIOD when switching topologies too
    updateSimulationVision();
    
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
    updatePreview();
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

void MosCircuitArchitect::updateSimulationVision() {
    // Look for active schematic editor to detect .tran timing
    SchematicEditor* editor = nullptr;
    for (auto* widget : QApplication::topLevelWidgets()) {
        if (auto* e = qobject_cast<SchematicEditor*>(widget)) {
            if (!editor || e->isActiveWindow()) editor = e;
        }
    }
    
    if (!editor) return;
    
    SchematicView* view = nullptr;
    QTabWidget* tabWidget = editor->findChild<QTabWidget*>();
    if (tabWidget) view = qobject_cast<SchematicView*>(tabWidget->currentWidget());
    if (!view) view = editor->findChild<SchematicView*>();
    
    if (view && view->scene()) {
        double tranStop = 0;
        for (auto* item : view->scene()->items()) {
            if (auto* directive = dynamic_cast<SchematicSpiceDirectiveItem*>(item)) {
                QString text = directive->text().toUpper();
                if (text.contains(".TRAN")) {
                    QStringList parts = text.split(QRegularExpression("[\\s,]+"), Qt::SkipEmptyParts);
                    if (parts.size() >= 3) {
                         SimValueParser::parseSpiceNumber(parts[2].toStdString(), tranStop);
                    }
                }
            }
        }
        if (tranStop > 0) {
            QString tranStr = QString::number(tranStop, 'g', 6);
            for (int i = 0; i < m_paramTable->rowCount(); ++i) {
                if (m_paramTable->item(i, 0)->text() == "PERIOD") {
                    m_paramTable->item(i, 1)->setText(tranStr);
                    break;
                }
            }
        }
    }
}

void MosCircuitArchitect::onGenerateClicked() {
    // This is now replaced by Apply & Run Simulation workflow
}

void MosCircuitArchitect::onVerifyClicked() {
    qDebug() << "[MosArchitect] Apply & Run Triggered.";
    
    // 1. Force vision sync and preview update
    updateSimulationVision();
    updatePreview();
    
    // 2. Perform Placement
    QGraphicsScene* scene = nullptr;
    QPointF placementPos;
    QUndoStack* stack = nullptr;
    SchematicView* activeView = nullptr;
    SchematicEditor* editor = nullptr;

    if (m_sourceItem && m_sourceItem->scene()) {
        scene = m_sourceItem->scene();
        placementPos = m_sourceItem->pos();
        if (!scene->views().isEmpty()) {
            activeView = qobject_cast<SchematicView*>(scene->views().first());
        }
    } else {
        // Search all top-level windows for an editor
        for (auto* widget : QApplication::topLevelWidgets()) {
            if (auto* e = qobject_cast<SchematicEditor*>(widget)) {
                if (!editor || e->isActiveWindow()) editor = e;
            }
        }
        
        if (editor) {
            QTabWidget* tabWidget = editor->findChild<QTabWidget*>();
            if (tabWidget) activeView = qobject_cast<SchematicView*>(tabWidget->currentWidget());
            if (!activeView) activeView = editor->findChild<SchematicView*>();
        }
        
        if (activeView && activeView->scene()) {
            scene = activeView->scene();
            placementPos = activeView->mapToScene(activeView->viewport()->rect().center());
        }
    }

    if (scene) {
        QString topoName = m_currentSynthesizer->getTopologyName();
        QString typeName = "HALF_BRIDGE_POWER_STAGE";
        if (topoName.contains("Push-Pull")) typeName = "PUSH_PULL_POWER_STAGE";
        else if (topoName.contains("Matrix")) typeName = "MATRIX_POWER_STAGE";
        
        QString refBase = m_sourceItem ? m_sourceItem->reference() : "MOS1";
        if (refBase.startsWith("V")) refBase = "M" + refBase.mid(1);
        
        // Unify model and symbol name to ensure correct SPICE netlisting
        QString modelName = typeName + "_" + refBase;
        
        auto* powerBlock = SchematicItemFactory::instance().createItem(typeName, placementPos, {}, nullptr);
        if (!powerBlock) return;
        
        powerBlock->setReference("X" + refBase);
        powerBlock->setValue(modelName);
        if (auto* generic = dynamic_cast<GenericComponentItem*>(powerBlock)) {
            auto def = generic->symbol();
            def.setName(modelName);      // Used for instance name
            def.setModelName(modelName); // Internal consistency
            generic->setSymbol(def);
        }
        
        QString subckt = m_previewArea->toPlainText();
        subckt.replace("CUSTOM_WAVE_GEN", modelName);
        
        auto* directive = new SchematicSpiceDirectiveItem(subckt);
        directive->setPos(placementPos + QPointF(120, 100));
        
        if (activeView) stack = activeView->undoStack();
        if (stack) {
            stack->beginMacro("Apply Power MOS Stage");
            stack->push(new AddItemCommand(scene, powerBlock));
            stack->push(new AddItemCommand(scene, directive));
            if (m_sourceItem) stack->push(new RemoveItemCommand(scene, {m_sourceItem}));
            stack->endMacro();
        } else {
            scene->addItem(powerBlock);
            scene->addItem(directive);
        }
        
        scene->clearSelection();
        powerBlock->setSelected(true);
        if (activeView) activeView->centerOn(powerBlock);

        // 3. Trigger Global Simulation
        // Find editor if we don't have it
        if (!editor) {
            for (auto* widget : QApplication::topLevelWidgets()) {
                if (auto* e = qobject_cast<SchematicEditor*>(widget)) {
                    if (e->centralWidget()->findChild<SchematicView*>() == activeView) {
                        editor = e;
                        break;
                    }
                }
            }
        }
        
        if (editor) {
            qDebug() << "[MosArchitect] Triggering global simulation in editor.";
            QMetaObject::invokeMethod(editor, "onRunSimulation");
        } else {
             qDebug() << "[MosArchitect] Error: Could not find editor to trigger simulation.";
        }

        close();
    } else {
        QMessageBox::warning(this, "No Active Schematic", "Open a schematic first!");
    }
}

void MosCircuitArchitect::onSquareClicked() {
    bool ok;
    double duty = QInputDialog::getDouble(this, "Square Wave", "Duty Cycle (0.1 - 0.9):", 0.5, 0.1, 0.9, 2, &ok);
    if (!ok) return;
    auto pts = WaveformEngine::generateSquare(duty);
    m_drawWidget->setPoints(pts);
    m_drawWidget->setStepMode(true);
    if (m_stepCheck) m_stepCheck->setChecked(true);
    updatePreview();
}

void MosCircuitArchitect::onBitstreamClicked() {
    bool ok;
    QString bits = QInputDialog::getText(this, "Bitstream", "Enter Bits (e.g. 10n110):", QLineEdit::Normal, m_lastBitstream.isEmpty() ? "101010" : m_lastBitstream, &ok);
    if (!ok || bits.isEmpty()) return;
    m_lastBitstream = bits;
    auto pts = WaveformEngine::generateBitstream(bits);
    m_drawWidget->setPoints(pts);
    m_drawWidget->setStepMode(true);
    if (m_stepCheck) m_stepCheck->setChecked(true);
    updatePreview();
}

void MosCircuitArchitect::setSourceItem(VoltageSourceItem* item) {
    m_sourceItem = item;
    if (m_sourceItem) {
        setWindowTitle("Convert to Power MOS Stage - " + m_sourceItem->reference());
        auto pts = WaveformEngine::pointsFromVoltageSource(m_sourceItem);
        if (!pts.isEmpty()) m_drawWidget->setPoints(pts);
        updateSimulationVision();
        updatePreview();
    }
}

void MosCircuitArchitect::setPoints(const QVector<QPointF>& points) {
    m_drawWidget->setPoints(points);
    updatePreview();
}
