#include "simulation_panel.h"
#include "../items/voltage_source_item.h"
#include "../items/schematic_spice_directive_item.h"
#include "../items/schematic_page_item.h"
#include "../items/simulation_net_table_item.h"
#include "../items/wire_item.h"
#include "../../simulator/core/sim_value_parser.h"
#include "simulator/core/raw_data_parser.h"
#include "waveform_viewer.h"
#include "simulation_log_dialog.h"
#include "../../simulator/bridge/sim_audio_engine.h"
#include "theme_manager.h"
#include "config_manager.h"
#include "simulation_manager.h"
#include "../io/netlist_generator.h"
#include "../items/schematic_item.h"
#include "../analysis/spice_netlist_generator.h"
#include "../io/schematic_file_io.h"
#include "../../simulator/bridge/sim_schematic_bridge.h"
#include "../analysis/net_manager.h"
#include "../editor/schematic_editor.h"
#include "../dialogs/spice_step_dialog.h"
#include "../dialogs/pre_simulation_validation_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDebug>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTemporaryFile>
#include <QTextStream>
#include <QTimer>
#include <QDialog>
#include <QDialogButtonBox>
#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QValueAxis>
#include <QLogValueAxis>
#include <QCategoryAxis>
#include <QListWidget>
#include <QSplitter>
#include <QGraphicsView>
#include <QCheckBox>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QToolBar>
#include <QTabWidget>
#include <QScrollArea>
#include <QFileDialog>
#include <QMenu>
#include <QColorDialog>
#include <QInputDialog>
#include <QApplication>
#include <QClipboard>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QShortcut>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QTableWidget>
#include <QSet>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include "virtual_instruments.h"
#include "si_formatter.h"
#include "../../simulator/core/sim_math.h"

void SimulationPanel::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    PCBTheme* theme = ThemeManager::theme();
    QString bg = theme ? theme->windowBackground().name() : "#1e1e1e";
    QString panelBg = theme ? theme->panelBackground().name() : "#252526";
    QString textColor = theme ? theme->textColor().name() : "#cccccc";
    QString accent = theme ? theme->accentColor().name() : "#3b82f6";
    QString borderColor = theme ? theme->panelBorder().name() : "#3c3c3c";
    const bool isLight = theme && theme->type() == PCBTheme::Light;
    const QString inputBg = isLight ? "#ffffff" : "#121214";
    const QString mutedText = theme ? theme->textSecondary().name() : "#888888";
    const QString chartBg = isLight ? "#ffffff" : "#000000";
    const QString chartPlotBg = isLight ? "#f8fafc" : "#000000";
    auto buttonStyle = [&](const QString& bgColor, const QString& fgColor) {
        return QString("background-color: %1; color: %2; font-weight: bold; padding: 4px 10px; border-radius: 4px; border: 1px solid %3;")
            .arg(bgColor, fgColor, borderColor);
    };
    const QString checkboxStyle = QString("QCheckBox { color: %1; font-weight: bold; }").arg(textColor);
    const QString inputStyle = QString("QLineEdit, QComboBox, QDoubleSpinBox { background: %1; color: %2; border: 1px solid %3; }")
        .arg(inputBg, textColor, borderColor);
    const QString commandStyle = QString("QLineEdit { background: %1; color: %2; border: 1px solid %2; font-family: 'Courier New'; font-weight: bold; }")
        .arg(isLight ? "#eff6ff" : "#1e3a5f", accent);

    setObjectName("SimulationPanel");
    setStyleSheet(QString("#SimulationPanel { background-color: %1; }").arg(bg));

    // --- Toolbar ---
    QToolBar* toolbar = new QToolBar("Simulation Controls");
    toolbar->setIconSize(QSize(20, 20));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->setStyleSheet(QString(
        "QToolBar { background-color: %1; border-bottom: 1px solid %2; padding: 4px; spacing: 8px; }"
        "QToolButton { background: %3; border: 1px solid %2; border-radius: 2px; padding: 4px 8px; font-weight: bold; color: %4; }"
        "QToolButton:hover { background: %5; }"
    ).arg(panelBg, borderColor, bg, textColor, accent));

    m_runButton = new QPushButton("Run Simulation");
    m_runButton->setStyleSheet(buttonStyle("#065f46", "white"));
    connect(m_runButton, &QPushButton::clicked, this, &SimulationPanel::onRunSimulation);
    toolbar->addWidget(m_runButton);

    QPushButton* exportCsvResultsBtn = new QPushButton("Export Waves CSV");
    exportCsvResultsBtn->setStyleSheet(buttonStyle(isLight ? "#e2e8f0" : "#1f2937", isLight ? textColor : "white"));
    connect(exportCsvResultsBtn, &QPushButton::clicked, this, &SimulationPanel::onExportResultsCsv);
    toolbar->addWidget(exportCsvResultsBtn);

    QPushButton* exportJsonResultsBtn = new QPushButton("Export Results JSON");
    exportJsonResultsBtn->setStyleSheet(buttonStyle(isLight ? "#e2e8f0" : "#1f2937", isLight ? textColor : "white"));
    connect(exportJsonResultsBtn, &QPushButton::clicked, this, &SimulationPanel::onExportResultsJson);
    toolbar->addWidget(exportJsonResultsBtn);

    QPushButton* exportReportBtn = new QPushButton("Export Report");
    exportReportBtn->setStyleSheet(buttonStyle("#0f766e", "white"));
    connect(exportReportBtn, &QPushButton::clicked, this, &SimulationPanel::onExportResultsReport);
    toolbar->addWidget(exportReportBtn);

    m_overlayPreviousRun = new QCheckBox("Overlay Previous");
    m_overlayPreviousRun->setStyleSheet(checkboxStyle);
    connect(m_overlayPreviousRun, &QCheckBox::toggled, this, [this](bool) {
        if (m_hasLastResults) plotBuiltinResults(m_lastResults);
    });
    toolbar->addWidget(m_overlayPreviousRun);

    QPushButton* probeBtn = new QPushButton("Add Probe");
    probeBtn->setStyleSheet(buttonStyle("#1e40af", "white"));
    connect(probeBtn, &QPushButton::clicked, this, &SimulationPanel::probeRequested);
    toolbar->addWidget(probeBtn);

    QPushButton* probePinBtn = new QPushButton("Probe On Canvas");
    probePinBtn->setStyleSheet(buttonStyle("#1d4ed8", "white"));
    connect(probePinBtn, &QPushButton::clicked, this, [this]() { Q_EMIT placementToolRequested("Probe"); });
    toolbar->addWidget(probePinBtn);

    QPushButton* placeScopeBtn = new QPushButton("Place Scope");
    placeScopeBtn->setStyleSheet(buttonStyle("#0f766e", "white"));
    connect(placeScopeBtn, &QPushButton::clicked, this, [this]() { Q_EMIT placementToolRequested("Oscilloscope Instrument"); });
    toolbar->addWidget(placeScopeBtn);

    QPushButton* placeMeterBtn = new QPushButton("Place Voltmeter");
    placeMeterBtn->setStyleSheet(buttonStyle("#0f766e", "white"));
    connect(placeMeterBtn, &QPushButton::clicked, this, [this]() { Q_EMIT placementToolRequested("Voltmeter (DC)"); });
    toolbar->addWidget(placeMeterBtn);

    QPushButton* removeProbeBtn = new QPushButton("Remove Probe");
    removeProbeBtn->setStyleSheet(buttonStyle("#9a3412", "white"));
    connect(removeProbeBtn, &QPushButton::clicked, this, [this]() {
        QListWidgetItem* item = m_signalList ? m_signalList->currentItem() : nullptr;
        if (!item) {
            m_logOutput->append("No selected probe to remove.");
            return;
        }
        removeProbe(item->text());
    });
    toolbar->addWidget(removeProbeBtn);

    QPushButton* clearProbesBtn = new QPushButton("Clear Probes");
    clearProbesBtn->setStyleSheet(buttonStyle(isLight ? "#cbd5e1" : "#4b5563", isLight ? textColor : "white"));
    connect(clearProbesBtn, &QPushButton::clicked, this, &SimulationPanel::clearAllProbes);
    toolbar->addWidget(clearProbesBtn);

    toolbar->addSeparator();

    QCheckBox* showVoltageCheck = new QCheckBox("Voltages");
    showVoltageCheck->setChecked(true);
    showVoltageCheck->setStyleSheet(checkboxStyle);
    toolbar->addWidget(showVoltageCheck);

    QCheckBox* showCurrentCheck = new QCheckBox("Currents");
    showCurrentCheck->setChecked(true);
    showCurrentCheck->setStyleSheet(checkboxStyle);
    toolbar->addWidget(showCurrentCheck);

    auto updateOverlays = [this, showVoltageCheck, showCurrentCheck]() {
        Q_EMIT overlayVisibilityChanged(showVoltageCheck->isChecked(), showCurrentCheck->isChecked());
    };
    connect(showVoltageCheck, &QCheckBox::toggled, this, updateOverlays);
    connect(showCurrentCheck, &QCheckBox::toggled, this, updateOverlays);

    QPushButton* clearOverlaysBtn = new QPushButton("Clear Overlays");
    clearOverlaysBtn->setStyleSheet(buttonStyle(isLight ? "#e2e8f0" : "#1f2937", isLight ? textColor : "white"));
    connect(clearOverlaysBtn, &QPushButton::clicked, this, &SimulationPanel::clearOverlaysRequested);
    toolbar->addWidget(clearOverlaysBtn);

    QCheckBox* topNetTableCheck = new QCheckBox("Net Table");
    topNetTableCheck->setChecked(true);
    topNetTableCheck->setStyleSheet(checkboxStyle);
    toolbar->addWidget(topNetTableCheck);

    toolbar->addSeparator();

    QPushButton* netlistBtn = new QPushButton("View Netlist");
    connect(netlistBtn, &QPushButton::clicked, this, &SimulationPanel::onViewNetlist);
    toolbar->addWidget(netlistBtn);

    toolbar->addSeparator();

    QPushButton* listenBtn = new QPushButton("Listen");
    listenBtn->setToolTip("Play selected signal through speakers (Transient only)");
    listenBtn->setStyleSheet(buttonStyle("#7c3aed", "white"));
    connect(listenBtn, &QPushButton::clicked, this, [this]() {
        if (!m_signalList || !m_signalList->currentItem()) return;
        QString name = m_signalList->currentItem()->text();
        for (const auto& w : m_lastResults.waveforms) {
            if (QString::fromStdString(w.name) == name) {
                SimAudioEngine::instance().playWaveform(w);
                break;
            }
        }
    });
    toolbar->addWidget(listenBtn);

    mainLayout->addWidget(toolbar);

    QSplitter* mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainSplitter->setHandleWidth(2);
    mainSplitter->setStyleSheet(QString("QSplitter::handle { background-color: %1; }").arg(borderColor));

    // --- Sidebar ---
    QWidget* sidebar = new QWidget();
    sidebar->setMinimumWidth(240);
    sidebar->setStyleSheet(QString("QWidget { background-color: %1; }").arg(panelBg));
    QVBoxLayout* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(8, 12, 8, 12);
    sidebarLayout->setSpacing(15);

    m_schematicNameLabel = new QLabel("SOURCE: None");
    m_schematicNameLabel->setStyleSheet(QString("QLabel { font-weight: bold; font-size: 11px; color: %1; background-color: %2; padding: 6px; border-radius: 4px; border: 1px solid %3; }")
                                          .arg(accent, bg, borderColor));
    m_schematicNameLabel->setAlignment(Qt::AlignCenter);
    sidebarLayout->addWidget(m_schematicNameLabel);

    QLabel* settingsLabel = new QLabel("ANALYSIS SETUP");
    settingsLabel->setStyleSheet(QString("QLabel { font-weight: bold; font-size: 10px; color: %1; letter-spacing: 1px; border: none; }").arg(theme ? theme->textSecondary().name() : "#888"));
    sidebarLayout->addWidget(settingsLabel);

    QFrame* configFrame = new QFrame();
    configFrame->setStyleSheet(QString("QFrame { background: %1; border: 1px solid %2; border-radius: 3px; }").arg(bg, borderColor));
    QFormLayout* configForm = new QFormLayout(configFrame);
    configForm->setLabelAlignment(Qt::AlignRight);

    m_analysisType = new QComboBox();
    m_analysisType->addItems({"Transient", "DC OP", "AC Sweep", "RF S-Parameter", "Monte Carlo", "Parametric Sweep", "Sensitivity", "Real-time Mode"});
    m_analysisType->setCurrentIndex(1);
    m_analysisType->setStyleSheet(inputStyle);
    configForm->addRow("Type:", m_analysisType);

    m_commandLine = new QLineEdit(".op");
    m_commandLine->setStyleSheet(commandStyle);
    m_commandLine->setPlaceholderText(".tran <tstep> <tstop>");
    QWidget* commandRow = new QWidget();
    auto* commandRowLayout = new QHBoxLayout(commandRow);
    commandRowLayout->setContentsMargins(0, 0, 0, 0);
    commandRowLayout->setSpacing(6);
    commandRowLayout->addWidget(m_commandLine, 1);
    QPushButton* stepBuilderBtn = new QPushButton(".step...");
    stepBuilderBtn->setToolTip("Open the LTspice .step sweep builder");
    stepBuilderBtn->setStyleSheet(buttonStyle("#0f766e", "white"));
    connect(stepBuilderBtn, &QPushButton::clicked, this, &SimulationPanel::onOpenStepBuilder);
    commandRowLayout->addWidget(stepBuilderBtn);
    configForm->addRow("Command:", commandRow);

    m_param1 = new QLineEdit("1u");
    m_param2 = new QLineEdit("10m");
    m_param3 = new QLineEdit("0");
    m_param4 = new QLineEdit();
    m_param5 = new QLineEdit();
    m_param6 = new QLineEdit("50");
    m_steadyCheck = new QCheckBox("Stop at steady state");
    m_steadyTolEdit = new QLineEdit();
    m_steadyDelayEdit = new QLineEdit();
    m_autoNetTableCheck = new QCheckBox("Auto net table on transient run");
    bool autoNetDefault = ConfigManager::instance().toolProperty("SimulationPanel", "showTransientNetTable", true).toBool();
    m_autoNetTableCheck->setChecked(autoNetDefault);
    for(auto* l : {m_param1, m_param2, m_param3, m_param4, m_param5, m_param6}) l->setStyleSheet(inputStyle);
    for (auto* l : {m_steadyTolEdit, m_steadyDelayEdit}) l->setStyleSheet(inputStyle);
    m_steadyTolEdit->setPlaceholderText("0.01");
    m_steadyDelayEdit->setPlaceholderText("0");
    
    configForm->addRow("Step/Start:", m_param1);
    configForm->addRow("Stop:", m_param2);
    configForm->addRow("Start/Pts:", m_param3);
    configForm->addRow("Steady:", m_steadyCheck);
    configForm->addRow("ssTol:", m_steadyTolEdit);
    configForm->addRow("ssDelay:", m_steadyDelayEdit);
    configForm->addRow("P4/Src:", m_param4);
    configForm->addRow("P5/Node:", m_param5);
    configForm->addRow("Z0:", m_param6);
    sidebarLayout->addWidget(configFrame);

    QLabel* generatorsLabel = new QLabel("SOURCE GENERATORS");
    generatorsLabel->setStyleSheet(settingsLabel->styleSheet());
    sidebarLayout->addWidget(generatorsLabel);

    QFrame* generatorFrame = new QFrame();
    generatorFrame->setStyleSheet(QString("QFrame { background: %1; border: 1px solid %2; border-radius: 3px; }").arg(bg, borderColor));
    QFormLayout* generatorForm = new QFormLayout(generatorFrame);
    generatorForm->setLabelAlignment(Qt::AlignRight);

    m_generatorType = new QComboBox();
    m_generatorType->addItems({"DC", "SIN", "PULSE", "EXP", "SFFM", "PWL", "AM", "FM"});
    m_generatorType->setStyleSheet(inputStyle);
    generatorForm->addRow("Type:", m_generatorType);

    m_generatorPresetCombo = new QComboBox();
    m_generatorPresetCombo->setStyleSheet(inputStyle);
    generatorForm->addRow("Template:", m_generatorPresetCombo);

    m_genLabel1 = new QLabel("Value:");
    m_genLabel2 = new QLabel("P2:");
    m_genLabel3 = new QLabel("P3:");
    m_genLabel4 = new QLabel("P4:");
    m_genLabel5 = new QLabel("P5:");
    m_genLabel6 = new QLabel("P6:");

    m_genParam1 = new QLineEdit("5");
    m_genParam2 = new QLineEdit("1");
    m_genParam3 = new QLineEdit("1k");
    m_genParam4 = new QLineEdit("0");
    m_genParam5 = new QLineEdit("0");
    m_genParam6 = new QLineEdit("0");
    for (auto* l : {m_genParam1, m_genParam2, m_genParam3, m_genParam4, m_genParam5, m_genParam6}) {
        l->setStyleSheet(inputStyle);
    }

    generatorForm->addRow(m_genLabel1, m_genParam1);
    generatorForm->addRow(m_genLabel2, m_genParam2);
    generatorForm->addRow(m_genLabel3, m_genParam3);
    generatorForm->addRow(m_genLabel4, m_genParam4);
    generatorForm->addRow(m_genLabel5, m_genParam5);
    generatorForm->addRow(m_genLabel6, m_genParam6);

    QPushButton* applyGeneratorBtn = new QPushButton("Apply to Selected Source");
    applyGeneratorBtn->setStyleSheet(buttonStyle("#7c2d12", "white"));
    generatorForm->addRow("", applyGeneratorBtn);

    QWidget* waveTools = new QWidget();
    QGridLayout* waveToolsLayout = new QGridLayout(waveTools);
    waveToolsLayout->setContentsMargins(0, 0, 0, 0);
    waveToolsLayout->setHorizontalSpacing(6);
    waveToolsLayout->setVerticalSpacing(6);

    QPushButton* pwlEditorBtn = new QPushButton("PWL Editor");
    QPushButton* importCsvBtn = new QPushButton("Import CSV");
    QPushButton* exportCsvBtn = new QPushButton("Export CSV");
    QPushButton* savePresetBtn = new QPushButton("Save Preset");
    QPushButton* deletePresetBtn = new QPushButton("Delete Preset");
    for (QPushButton* b : {pwlEditorBtn, importCsvBtn, exportCsvBtn, savePresetBtn, deletePresetBtn}) {
        b->setStyleSheet(buttonStyle(isLight ? "#e2e8f0" : "#374151", isLight ? textColor : "white"));
    }
    pwlEditorBtn->setStyleSheet(buttonStyle("#1e40af", "white"));
    savePresetBtn->setStyleSheet(buttonStyle("#065f46", "white"));
    deletePresetBtn->setStyleSheet(buttonStyle("#7f1d1d", "white"));

    waveToolsLayout->addWidget(pwlEditorBtn, 0, 0);
    waveToolsLayout->addWidget(importCsvBtn, 0, 1);
    waveToolsLayout->addWidget(exportCsvBtn, 1, 0);
    waveToolsLayout->addWidget(savePresetBtn, 1, 1);
    waveToolsLayout->addWidget(deletePresetBtn, 2, 0, 1, 2);
    generatorForm->addRow("", waveTools);
    sidebarLayout->addWidget(generatorFrame);

    QLabel* signalsLabel = new QLabel("TRACE MONITOR");
    signalsLabel->setStyleSheet(settingsLabel->styleSheet());
    sidebarLayout->addWidget(signalsLabel);

    m_signalList = new QListWidget();
    m_signalList->setStyleSheet(QString(
        "QListWidget { background: %1; border: 1px solid %2; border-radius: 3px; color: #eee; }"
        "QListWidget::item { padding: 4px; border-bottom: 1px solid %2; }"
        "QListWidget::item:selected { background: %3; color: white; }"
    ).arg(bg, borderColor, accent));
    m_signalList->setContextMenuPolicy(Qt::CustomContextMenu);
    sidebarLayout->addWidget(m_signalList, 1);

    QLabel* measurementsLabel = new QLabel("MEASUREMENTS");
    measurementsLabel->setStyleSheet(settingsLabel->styleSheet());
    sidebarLayout->addWidget(measurementsLabel);

    QWidget* cursorWidget = new QWidget();
    QHBoxLayout* cursorLayout = new QHBoxLayout(cursorWidget);
    cursorLayout->setContentsMargins(0, 0, 0, 0);
    cursorLayout->setSpacing(6);
    cursorLayout->addWidget(new QLabel("A%"));
    QDoubleSpinBox* cursorASpin = new QDoubleSpinBox();
    cursorASpin->setRange(0.0, 100.0);
    cursorASpin->setDecimals(1);
    cursorASpin->setSingleStep(1.0);
    cursorASpin->setValue(m_cursorAFrac * 100.0);
    cursorASpin->setStyleSheet("QDoubleSpinBox { background: #121214; color: #fff; border: 1px solid #333; }");
    cursorLayout->addWidget(cursorASpin);
    cursorLayout->addWidget(new QLabel("B%"));
    QDoubleSpinBox* cursorBSpin = new QDoubleSpinBox();
    cursorBSpin->setRange(0.0, 100.0);
    cursorBSpin->setDecimals(1);
    cursorBSpin->setSingleStep(1.0);
    cursorBSpin->setValue(m_cursorBFrac * 100.0);
    cursorBSpin->setStyleSheet("QDoubleSpinBox { background: #121214; color: #fff; border: 1px solid #333; }");
    cursorLayout->addWidget(cursorBSpin);
    sidebarLayout->addWidget(cursorWidget);

    connect(cursorASpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        m_cursorAFrac = std::clamp(v / 100.0, 0.0, 1.0);
    });
    connect(cursorBSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        m_cursorBFrac = std::clamp(v / 100.0, 0.0, 1.0);
    });

    m_measurementsTable = new QTableWidget(0, 6);
    m_measurementsTable->setHorizontalHeaderLabels({"Name", "Primary/Result", "Avg", "RMS", "Freq/Step", "Delta(A-B)"});
    m_measurementsTable->horizontalHeader()->setStretchLastSection(true);
    m_measurementsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_measurementsTable->verticalHeader()->hide();
    m_measurementsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_measurementsTable->setStyleSheet(QString(
        "QTableWidget { background: %1; border: 1px solid %2; color: %3; font-size: 9px; }"
        "QHeaderView::section { background: %4; border: 1px solid %2; color: %3; padding: 2px; }"
    ).arg(bg, borderColor, textColor, panelBg));
    sidebarLayout->addWidget(m_measurementsTable, 1);

    m_logOutput = new QTextEdit();
    m_logOutput->setReadOnly(true);
    m_logOutput->setMaximumHeight(100);
    m_logOutput->setStyleSheet(QString("QTextEdit { background: %1; border: 1px solid %2; font-family: monospace; font-size: 9px; color: #eee; }").arg(bg, borderColor));
    sidebarLayout->addWidget(m_logOutput);

    QPushButton* showFullLogBtn = new QPushButton("Show Detailed Log");
    showFullLogBtn->setStyleSheet("QPushButton { background: #374151; color: white; border-radius: 3px; padding: 4px; font-size: 10px; }");
    connect(showFullLogBtn, &QPushButton::clicked, this, &SimulationPanel::showDetailedLog);
    auto* logShortcut = new QShortcut(QKeySequence("Ctrl+L"), this);
    connect(logShortcut, &QShortcut::activated, this, &SimulationPanel::showDetailedLog);
    sidebarLayout->addWidget(showFullLogBtn);

    QLabel* issuesLabel = new QLabel("SIM ISSUES (DOUBLE-CLICK TO NAVIGATE)");
    issuesLabel->setStyleSheet(settingsLabel->styleSheet());
    sidebarLayout->addWidget(issuesLabel);

    m_issueList = new QListWidget();
    m_issueList->setStyleSheet(QString(
        "QListWidget { background: %1; border: 1px solid %2; border-radius: 3px; color: #fbbf24; }"
        "QListWidget::item { padding: 4px; border-bottom: 1px solid %2; }"
    ).arg(bg, borderColor));
    connect(m_issueList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        if (!item) return;
        const QString type = item->data(Qt::UserRole + 1).toString();
        const QString id = item->data(Qt::UserRole + 2).toString();
        if (!type.isEmpty() && !id.isEmpty()) {
            Q_EMIT simulationTargetRequested(type, id);
        }
    });
    sidebarLayout->addWidget(m_issueList, 1);

    QScrollArea* sidebarScroll = new QScrollArea();
    sidebarScroll->setWidgetResizable(true);
    sidebarScroll->setFrameShape(QFrame::NoFrame);
    sidebarScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sidebarScroll->setMinimumWidth(240);
    sidebarScroll->setStyleSheet(QString("QScrollArea { background-color: %1; border-right: 1px solid %2; }").arg(panelBg, borderColor));
    sidebarScroll->setWidget(sidebar);
    mainSplitter->addWidget(sidebarScroll);

    // --- Main Plot Content ---
    QWidget* plotContainer = new QWidget();
    QVBoxLayout* plotLayout = new QVBoxLayout(plotContainer);
    plotLayout->setContentsMargins(10, 10, 10, 10);

    // ── Time-Travel Timeline ───────────────────────────────────────────
    QWidget* timelineWidget = new QWidget();
    QHBoxLayout* timelineLayout = new QHBoxLayout(timelineWidget);
    timelineLayout->setContentsMargins(0, 0, 0, 0);
    
    QLabel* ttLabel = new QLabel("TIME-TRAVEL:");
    ttLabel->setStyleSheet(QString("font-weight: bold; font-size: 10px; color: %1;").arg(accent));
    timelineLayout->addWidget(ttLabel);
    
    m_timelineSlider = new QSlider(Qt::Horizontal);
    m_timelineSlider->setRange(0, 1000);
    m_timelineSlider->setEnabled(false);
    m_timelineSlider->setStyleSheet(QString("QSlider::handle:horizontal { background: %1; border-radius: 4px; width: 12px; }").arg(accent));
    timelineLayout->addWidget(m_timelineSlider, 1);
    
    m_timelineLabel = new QLabel("--- s");
    m_timelineLabel->setMinimumWidth(80);
    m_timelineLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_timelineLabel->setStyleSheet(QString("font-family: monospace; color: %1; font-size: 11px;").arg(textColor));
    timelineLayout->addWidget(m_timelineLabel);
    
    plotLayout->addWidget(timelineWidget);
    connect(m_timelineSlider, &QSlider::valueChanged, this, &SimulationPanel::onTimelineValueChanged);

    QWidget* netTableControls = new QWidget();
    auto* netTableLayout = new QHBoxLayout(netTableControls);
    netTableLayout->setContentsMargins(0, 0, 0, 0);
    netTableLayout->setSpacing(8);
    QLabel* netTableLabel = new QLabel("SCHEMATIC NET TABLE:");
    netTableLabel->setStyleSheet(QString("font-weight: bold; font-size: 10px; color: %1;").arg(accent));
    netTableLayout->addWidget(netTableLabel);
    m_autoNetTableCheck->setStyleSheet(checkboxStyle);
    netTableLayout->addWidget(m_autoNetTableCheck);
    netTableLayout->addSpacing(12);
    QLabel* plotQualityLabel = new QLabel("PLOT QUALITY:");
    plotQualityLabel->setStyleSheet(QString("font-weight: bold; font-size: 10px; color: %1;").arg(accent));
    netTableLayout->addWidget(plotQualityLabel);
    m_plotQualityCombo = new QComboBox();
    m_plotQualityCombo->addItems({"High Quality", "Balanced", "Fast Plotting"});
    m_plotQualityCombo->setStyleSheet(inputStyle);
    const QString savedPlotQuality = ConfigManager::instance().toolProperty(
        "SimulationPanel", "plotQuality", "Balanced").toString();
    const int savedPlotQualityIndex = m_plotQualityCombo->findText(savedPlotQuality);
    m_plotQualityCombo->setCurrentIndex(savedPlotQualityIndex >= 0 ? savedPlotQualityIndex : 1);
    netTableLayout->addWidget(m_plotQualityCombo);
    netTableLayout->addStretch(1);
    plotLayout->addWidget(netTableControls);

    connect(topNetTableCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (!m_autoNetTableCheck || m_autoNetTableCheck->isChecked() == checked) return;
        const QSignalBlocker blocker(m_autoNetTableCheck);
        m_autoNetTableCheck->setChecked(checked);
        Q_EMIT m_autoNetTableCheck->toggled(checked);
    });
    connect(m_autoNetTableCheck, &QCheckBox::toggled, this, [topNetTableCheck](bool checked) {
        if (topNetTableCheck->isChecked() == checked) return;
        const QSignalBlocker blocker(topNetTableCheck);
        topNetTableCheck->setChecked(checked);
    });

    m_chart = new QChart();
    m_chart->setTitle("Waveform Viewer");
    m_chart->setTitleBrush(QBrush(theme ? theme->textColor() : QColor(Qt::white)));
    m_chart->setBackgroundBrush(QBrush(QColor(chartBg)));
    m_chart->setPlotAreaBackgroundBrush(QBrush(QColor(chartPlotBg)));
    m_chart->setPlotAreaBackgroundVisible(true);
    m_chart->setMargins(QMargins(0, 0, 0, 0));
    m_chart->setBackgroundRoundness(0);
    
    m_chart->legend()->setVisible(true);
    m_chart->legend()->setAlignment(Qt::AlignTop);
    m_chart->legend()->setMarkerShape(QLegend::MarkerShapeRectangle);
    m_chart->legend()->setBackgroundVisible(false);
    m_chart->legend()->setLabelColor(theme ? theme->textColor() : QColor(Qt::white));
    
    m_plotView = new QChartView(m_chart);
    m_plotView->setStyleSheet(QString("background-color: %1; border: 1px solid %2;").arg(chartBg, borderColor));

    m_spectrumChart = new QChart();
    m_spectrumChart->setTitle("FFT Spectrum Analysis");
    m_spectrumChart->setTitleBrush(QBrush(theme ? theme->textColor() : QColor(Qt::white)));
    m_spectrumChart->setBackgroundBrush(QBrush(QColor(chartBg)));
    m_spectrumChart->setPlotAreaBackgroundBrush(QBrush(QColor(chartPlotBg)));
    m_spectrumChart->setPlotAreaBackgroundVisible(true);
    m_spectrumChart->setMargins(QMargins(0, 0, 0, 0));
    m_spectrumChart->setBackgroundRoundness(0);
    
    m_spectrumChart->legend()->setVisible(true);
    m_spectrumChart->legend()->setAlignment(Qt::AlignTop);
    m_spectrumChart->legend()->setBackgroundVisible(false);
    m_spectrumChart->legend()->setLabelColor(theme ? theme->textColor() : QColor(Qt::white));

    m_spectrumView = new QChartView(m_spectrumChart);
    m_spectrumView->setStyleSheet(m_plotView->styleSheet());

    m_spectrumTab = new QWidget();
    auto* spectrumTabLayout = new QVBoxLayout(m_spectrumTab);
    spectrumTabLayout->setContentsMargins(0, 0, 0, 0);
    spectrumTabLayout->setSpacing(6);

    QWidget* steppedControls = new QWidget();
    auto* steppedControlsLayout = new QHBoxLayout(steppedControls);
    steppedControlsLayout->setContentsMargins(0, 0, 0, 0);
    steppedControlsLayout->setSpacing(6);
    steppedControlsLayout->addWidget(new QLabel("Measurement"));
    m_steppedMeasSeriesCombo = new QComboBox();
    m_steppedMeasSeriesCombo->setEnabled(false);
    steppedControlsLayout->addWidget(m_steppedMeasSeriesCombo, 1);
    steppedControlsLayout->addWidget(new QLabel("X Axis"));
    m_steppedMeasAxisCombo = new QComboBox();
    m_steppedMeasAxisCombo->setEnabled(false);
    steppedControlsLayout->addWidget(m_steppedMeasAxisCombo, 1);
    spectrumTabLayout->addWidget(steppedControls);
    spectrumTabLayout->addWidget(m_spectrumView, 1);

    m_designExplorerTab = new QWidget();
    auto* explorerLayout = new QVBoxLayout(m_designExplorerTab);
    explorerLayout->setContentsMargins(8, 8, 8, 8);
    explorerLayout->setSpacing(8);
    m_designExplorerSummaryLabel = new QLabel("Design Explorer: no sweep, optimization, or sensitivity candidates in the current run.");
    m_designExplorerSummaryLabel->setWordWrap(true);
    m_designExplorerSummaryLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 12px; padding: 4px; }").arg(mutedText));
    explorerLayout->addWidget(m_designExplorerSummaryLabel);
    m_designExplorerDetailLabel = new QLabel("Select a case to inspect its assignments and metric values.");
    m_designExplorerDetailLabel->setWordWrap(true);
    m_designExplorerDetailLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 11px; padding: 4px; }").arg(textColor));
    explorerLayout->addWidget(m_designExplorerDetailLabel);
    QWidget* explorerActions = new QWidget();
    auto* explorerActionsLayout = new QHBoxLayout(explorerActions);
    explorerActionsLayout->setContentsMargins(0, 0, 0, 0);
    explorerActionsLayout->setSpacing(6);
    m_designExplorerCopyButton = new QPushButton("Copy Selected Case");
    m_designExplorerCopyButton->setEnabled(false);
    m_designExplorerCopyButton->setStyleSheet(buttonStyle("#1d4ed8", "white"));
    explorerActionsLayout->addWidget(m_designExplorerCopyButton);
    explorerActionsLayout->addStretch(1);
    explorerLayout->addWidget(explorerActions);
    m_designExplorerTable = new QTableWidget(0, 0);
    m_designExplorerTable->setSortingEnabled(true);
    m_designExplorerTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_designExplorerTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_designExplorerTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_designExplorerTable->verticalHeader()->hide();
    m_designExplorerTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_designExplorerTable->horizontalHeader()->setStretchLastSection(true);
    m_designExplorerTable->setStyleSheet(QString(
        "QTableWidget { background: %1; border: 1px solid %2; color: %3; font-size: 10px; }"
        "QHeaderView::section { background: %4; border: 1px solid %2; color: %3; padding: 3px; }"
    ).arg(bg, borderColor, textColor, panelBg));
    explorerLayout->addWidget(m_designExplorerTable, 1);

    m_viewTabs = new QTabWidget();
    m_viewTabs->setStyleSheet(QString("QTabWidget::pane { border: 1px solid %1; } QTabBar::tab { background: %2; color: %3; padding: 8px; } QTabBar::tab:selected { background: %4; }")
                            .arg(borderColor, panelBg, textColor, accent));

    m_rfTab = new QWidget();
    QVBoxLayout* rfLayout = new QVBoxLayout(m_rfTab);
    m_smithChart = new SmithChartWidget();
    rfLayout->addWidget(m_smithChart);
    m_viewTabs->addTab(m_rfTab, "RF Analysis");

    m_efficiencyTab = new QWidget();
    auto* efficiencyLayout = new QVBoxLayout(m_efficiencyTab);
    efficiencyLayout->setContentsMargins(8, 8, 8, 8);
    efficiencyLayout->setSpacing(8);
    m_efficiencySummaryLabel = new QLabel("No efficiency summary available for this run.");
    m_efficiencySummaryLabel->setWordWrap(true);
    m_efficiencySummaryLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 12px; padding: 4px; }").arg(mutedText));
    efficiencyLayout->addWidget(m_efficiencySummaryLabel);
    m_efficiencyTable = new QTableWidget(0, 2);
    m_efficiencyTable->setHorizontalHeaderLabels({"Metric", "Value"});
    m_efficiencyTable->horizontalHeader()->setStretchLastSection(true);
    m_efficiencyTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_efficiencyTable->verticalHeader()->hide();
    m_efficiencyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_efficiencyTable->setStyleSheet(QString(
        "QTableWidget { background: %1; border: 1px solid %2; color: %3; font-size: 10px; }"
        "QHeaderView::section { background: %4; border: 1px solid %2; color: %3; padding: 3px; }"
    ).arg(bg, borderColor, textColor, panelBg));
    efficiencyLayout->addWidget(m_efficiencyTable, 1);
    m_viewTabs->addTab(m_efficiencyTab, "Efficiency");
    
    m_waveformViewer = new WaveformViewer();
    m_scopeContainer = m_waveformViewer;
    
    m_viewTabs->addTab(m_plotView, "Standard Waves");
    m_viewTabs->addTab(m_spectrumTab, "FFT Spectrum");
    m_viewTabs->addTab(m_designExplorerTab, "Design Explorer");
    // viewTabs->addTab(m_waveformViewer, "Oscilloscope"); // Handled via bottom dock
    
    m_logicAnalyzer = new LogicAnalyzerWidget();
    m_viewTabs->addTab(m_logicAnalyzer, "Logic Analyzer");

    m_voltmeter = new VoltmeterWidget();
    m_viewTabs->addTab(m_voltmeter, "Voltmeter");

    m_ammeter = new AmmeterWidget();
    m_viewTabs->addTab(m_ammeter, "Ammeter");

    m_wattmeter = new WattmeterWidget();
    m_viewTabs->addTab(m_wattmeter, "Wattmeter");

    m_freqCounter = new FrequencyCounterWidget();
    m_viewTabs->addTab(m_freqCounter, "Frequency Counter");

    m_logicProbe = new LogicProbeWidget();
    m_viewTabs->addTab(m_logicProbe, "Logic Probe");

    plotLayout->addWidget(m_viewTabs);

    mainSplitter->addWidget(plotContainer);
    mainSplitter->setSizes({260, 600});
    mainLayout->addWidget(mainSplitter);

    connect(m_analysisType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SimulationPanel::onAnalysisChanged);
    connect(m_analysisType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SimulationPanel::updateSchematicDirective);
    connect(m_analysisType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SimulationPanel::updateCommandDisplay);
    connect(m_param1, &QLineEdit::textChanged, this, &SimulationPanel::updateSchematicDirective);
    connect(m_param2, &QLineEdit::textChanged, this, &SimulationPanel::updateSchematicDirective);
    connect(m_param3, &QLineEdit::textChanged, this, &SimulationPanel::updateSchematicDirective);
    connect(m_steadyCheck, &QCheckBox::toggled, this, &SimulationPanel::updateSchematicDirective);
    connect(m_steadyTolEdit, &QLineEdit::textChanged, this, &SimulationPanel::updateSchematicDirective);
    connect(m_steadyDelayEdit, &QLineEdit::textChanged, this, &SimulationPanel::updateSchematicDirective);
    connect(m_autoNetTableCheck, &QCheckBox::toggled, this, [this](bool enabled) {
        ConfigManager::instance().setToolProperty("SimulationPanel", "showTransientNetTable", enabled);
        if (!enabled) {
            clearTransientNetTableOverlay();
            if (m_scene) {
                QPointer<SimulationNetTableItem> table = m_netTableItems.take(m_scene);
                if (table) {
                    if (table->scene()) table->scene()->removeItem(table);
                    table->deleteLater();
                }
            }
        } else if (enabled && m_hasLastResults && m_lastResults.analysisType == SimAnalysisType::Transient) {
            updateTransientNetTableOverlay(m_lastResults);
        }
    });
    connect(m_plotQualityCombo, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        ConfigManager::instance().setToolProperty("SimulationPanel", "plotQuality", text);
        applyPlotQuality();
        if (m_hasLastResults) {
            plotBuiltinResults(m_lastResults);
        } else if (m_waveformViewer) {
            m_waveformViewer->updatePlot(false);
        }
    });
    connect(m_param1, &QLineEdit::textChanged, this, &SimulationPanel::updateCommandDisplay);
    connect(m_param2, &QLineEdit::textChanged, this, &SimulationPanel::updateCommandDisplay);
    connect(m_param3, &QLineEdit::textChanged, this, &SimulationPanel::updateCommandDisplay);
    connect(m_steadyCheck, &QCheckBox::toggled, this, &SimulationPanel::updateCommandDisplay);
    connect(m_steadyTolEdit, &QLineEdit::textChanged, this, &SimulationPanel::updateCommandDisplay);
    connect(m_steadyDelayEdit, &QLineEdit::textChanged, this, &SimulationPanel::updateCommandDisplay);
    connect(m_commandLine, &QLineEdit::editingFinished, this, [this]() {
        parseCommandText(m_commandLine->text());
    });
    connect(m_generatorType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SimulationPanel::onGeneratorTypeChanged);
    connect(m_generatorPresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SimulationPanel::onGeneratorPresetActivated);
    connect(applyGeneratorBtn, &QPushButton::clicked, this, &SimulationPanel::onApplyGeneratorToSelection);
    connect(pwlEditorBtn, &QPushButton::clicked, this, &SimulationPanel::onOpenPwlEditor);
    connect(importCsvBtn, &QPushButton::clicked, this, &SimulationPanel::onImportPwlCsv);
    connect(exportCsvBtn, &QPushButton::clicked, this, &SimulationPanel::onExportPwlCsv);
    connect(savePresetBtn, &QPushButton::clicked, this, &SimulationPanel::onSaveGeneratorPreset);
    connect(deletePresetBtn, &QPushButton::clicked, this, &SimulationPanel::onDeleteGeneratorPreset);
    connect(m_signalList, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        QString seriesName = item->text();
        bool isVisible = (item->checkState() == Qt::Checked);
        if (isVisible) {
            m_persistentCheckedSignals.insert(seriesName);
        } else {
            m_persistentCheckedSignals.remove(seriesName);
        }
        if (m_waveformViewer) {
            m_waveformViewer->setSignalChecked(seriesName, isVisible);
            m_waveformViewer->updatePlot(false);
        }
        if (m_chart) {
            for (auto* series : m_chart->series()) {
                if (series->name() == seriesName) {
                    series->setVisible(isVisible);
                    break;
                }
            }
        }
    });
    connect(m_signalList, &QListWidget::customContextMenuRequested,
            this, &SimulationPanel::onSignalListContextMenuRequested);
    connect(m_steppedMeasSeriesCombo, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        m_selectedSteppedMeasurement = text;
        if (m_hasLastResults) {
            refreshSteppedMeasurementControls(m_lastResults);
            rebuildSteppedMeasurementPlot(m_lastResults);
            refreshDesignExplorer(m_lastResults);
        }
    });
    connect(m_steppedMeasAxisCombo, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        m_selectedSteppedAxis = text;
        if (m_hasLastResults) rebuildSteppedMeasurementPlot(m_lastResults);
    });
    if (m_designExplorerTable && m_designExplorerTable->horizontalHeader()) {
        connect(m_designExplorerTable->horizontalHeader(), &QHeaderView::sectionClicked, this, [this](int section) {
            if (!m_designExplorerTable || !m_steppedMeasSeriesCombo) return;
            auto* headerItem = m_designExplorerTable->horizontalHeaderItem(section);
            if (!headerItem) return;
            const QString metricName = headerItem->data(Qt::UserRole).toString();
            if (metricName.isEmpty()) return;
            m_selectedSteppedMeasurement = metricName;
            m_steppedMeasSeriesCombo->setCurrentText(metricName);
            if (m_viewTabs && m_spectrumTab) m_viewTabs->setCurrentWidget(m_spectrumTab);
        });
    }
    if (m_designExplorerTable) {
        connect(m_designExplorerTable, &QTableWidget::itemSelectionChanged, this, [this]() {
            if (m_hasLastResults) refreshDesignExplorerSelection(m_lastResults);
        });
    }
    if (m_designExplorerCopyButton) {
        connect(m_designExplorerCopyButton, &QPushButton::clicked, this, [this]() {
            if (!m_designExplorerTable) return;
            const int row = m_designExplorerTable->currentRow();
            if (row < 0) return;
            auto* caseItem = m_designExplorerTable->item(row, 1);
            if (!caseItem) return;
            const QString copyText = caseItem->data(Qt::UserRole + 2).toString();
            if (copyText.isEmpty()) return;
            QApplication::clipboard()->setText(copyText);
            m_logOutput->append(QString("Design Explorer: copied case %1 to clipboard.").arg(caseItem->text()));
        });
    }
    connect(m_viewTabs, &QTabWidget::currentChanged, this, [this](int) {
        if (!m_hasLastResults) return;
        if (shouldBuildStandardChart() && m_chart && m_chart->series().isEmpty()) {
            plotBuiltinResults(m_lastResults);
            return;
        }
        if (shouldBuildSpectrumChart() && m_spectrumChart && m_spectrumChart->series().isEmpty()) {
            plotBuiltinResults(m_lastResults);
        }
    });

    onGeneratorTypeChanged(m_generatorType->currentIndex());
    loadGeneratorLibrary();
    applyPlotQuality();
    updateCommandDisplay();
}

void SimulationPanel::onViewNetlist() {
    QDialog* dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle("Generated SPICE Netlist");
    dlg->resize(600, 500);
    if (ThemeManager::theme()) {
        dlg->setStyleSheet(ThemeManager::theme()->widgetStylesheet());
    }
    QVBoxLayout* lay = new QVBoxLayout(dlg);
    QTextEdit* edit = new QTextEdit;
    edit->setReadOnly(true);
    edit->setPlainText(generateSpiceNetlist());
    edit->setStyleSheet("font-family: monospace;");
    lay->addWidget(edit);
    dlg->show();
}

void SimulationPanel::onAnalysisChanged(int index) {
    SimManager::instance().stopRealTime();
    Q_EMIT analysisModeChanged();

    QFormLayout* layout = qobject_cast<QFormLayout*>(m_param1->parentWidget()->layout());
    if (!layout) return;
    
    auto setLabel = [&](QLineEdit* field, const QString& text) {
        if (auto* lbl = qobject_cast<QLabel*>(layout->labelForField(field)))
            lbl->setText(text);
    };

    if (index == 0) { // Transient
        setLabel(m_param1, "Step:");
        setLabel(m_param2, "Stop Time:");
        setLabel(m_param3, "Start Time:");
        setLabel(m_steadyTolEdit, "ssTol:");
        setLabel(m_steadyDelayEdit, "ssDelay:");
        m_param1->setVisible(true); m_param2->setVisible(true); m_param3->setVisible(true);
        m_steadyCheck->setVisible(true); m_steadyTolEdit->setVisible(true); m_steadyDelayEdit->setVisible(true);
        m_param4->setVisible(false); m_param5->setVisible(false); m_param6->setVisible(false);
        m_param1->setText("1u"); m_param2->setText("10m"); m_param3->setText("0");
    } else if (index == 1) { // DC OP
        m_param1->setVisible(false); m_param2->setVisible(false); m_param3->setVisible(false);
        m_steadyCheck->setVisible(false); m_steadyTolEdit->setVisible(false); m_steadyDelayEdit->setVisible(false);
        m_param4->setVisible(false); m_param5->setVisible(false); m_param6->setVisible(false);
    } else if (index == 2) { // AC Sweep
        setLabel(m_param1, "Start Freq:");
        setLabel(m_param2, "Stop Freq:");
        setLabel(m_param3, "Points/Dec:");
        m_param1->setVisible(true); m_param2->setVisible(true); m_param3->setVisible(true);
        m_steadyCheck->setVisible(false); m_steadyTolEdit->setVisible(false); m_steadyDelayEdit->setVisible(false);
        m_param4->setVisible(false); m_param5->setVisible(false); m_param6->setVisible(false);
        m_param1->setText("10"); m_param2->setText("1Meg"); m_param3->setText("10");
    } else if (index == 3) { // RF S-Parameter
        setLabel(m_param1, "Start Freq:");
        setLabel(m_param2, "Stop Freq:");
        setLabel(m_param3, "Points/Dec:");
        setLabel(m_param4, "Port 1 Src:");
        setLabel(m_param5, "Port 2 Node:");
        setLabel(m_param6, "Ref Z0:");
        m_param1->setVisible(true); m_param2->setVisible(true); m_param3->setVisible(true);
        m_steadyCheck->setVisible(false); m_steadyTolEdit->setVisible(false); m_steadyDelayEdit->setVisible(false);
        m_param4->setVisible(true); m_param5->setVisible(true); m_param6->setVisible(true);
        m_param1->setText("10"); m_param2->setText("1Meg"); m_param3->setText("10");
        m_param4->setText("V1"); m_param5->setText("OUT"); m_param6->setText("50");
    } else if (index == 4) { // Monte Carlo
        setLabel(m_param1, "Runs:");
        m_param1->setVisible(true); m_param2->setVisible(false); m_param3->setVisible(false);
        m_steadyCheck->setVisible(false); m_steadyTolEdit->setVisible(false); m_steadyDelayEdit->setVisible(false);
        m_param4->setVisible(false); m_param5->setVisible(false); m_param6->setVisible(false);
        m_param1->setText("10");
    } else if (index == 5) { // Parametric Sweep
        setLabel(m_param1, "Component:");
        setLabel(m_param2, "Param:");
        setLabel(m_param3, "Start:");
        setLabel(m_param4, "Stop:");
        setLabel(m_param5, "Steps:");
        m_param1->setVisible(true); m_param2->setVisible(true); m_param3->setVisible(true);
        m_steadyCheck->setVisible(false); m_steadyTolEdit->setVisible(false); m_steadyDelayEdit->setVisible(false);
        m_param4->setVisible(true); m_param5->setVisible(true); m_param6->setVisible(false);
        m_param1->setText("R1"); m_param2->setText("resistance"); m_param3->setText("1k");
        m_param4->setText("10k"); m_param5->setText("10");
    } else if (index == 6) { // Sensitivity
        setLabel(m_param1, "Target Signal:");
        m_param1->setVisible(true); m_param2->setVisible(false); m_param3->setVisible(false);
        m_steadyCheck->setVisible(false); m_steadyTolEdit->setVisible(false); m_steadyDelayEdit->setVisible(false);
        m_param4->setVisible(false); m_param5->setVisible(false); m_param6->setVisible(false);
        m_param1->setText("V(Out)");
    } else if (index == 7) { // Real-time
        setLabel(m_param1, "Update (ms):");
        m_param1->setVisible(true); m_param2->setVisible(false); m_param3->setVisible(false);
        m_steadyCheck->setVisible(false); m_steadyTolEdit->setVisible(false); m_steadyDelayEdit->setVisible(false);
        m_param4->setVisible(false); m_param5->setVisible(false); m_param6->setVisible(false);
        m_param1->setText("100");
    }
}

void SimulationPanel::onGeneratorTypeChanged(int index) {
    Q_UNUSED(index)
    if (!m_generatorType) return;
    const QString type = m_generatorType->currentText();

    auto showParam = [](QLabel* lbl, QLineEdit* edit, const QString& title, const QString& value) {
        lbl->setVisible(true);
        edit->setVisible(true);
        lbl->setText(title);
        if (edit->text().isEmpty()) {
            edit->setText(value);
        }
    };
    auto hideParam = [](QLabel* lbl, QLineEdit* edit) {
        lbl->setVisible(false);
        edit->setVisible(false);
    };

    if (type == "DC") {
        showParam(m_genLabel1, m_genParam1, "Value:", "5");
        hideParam(m_genLabel2, m_genParam2); hideParam(m_genLabel3, m_genParam3);
        hideParam(m_genLabel4, m_genParam4); hideParam(m_genLabel5, m_genParam5);
        hideParam(m_genLabel6, m_genParam6);
    } else if (type == "SIN") {
        showParam(m_genLabel1, m_genParam1, "Offset:", "0");
        showParam(m_genLabel2, m_genParam2, "Amplitude (Peak):", "5");
        showParam(m_genLabel3, m_genParam3, "Freq:", "1k");
        showParam(m_genLabel4, m_genParam4, "Delay:", "0");
        showParam(m_genLabel5, m_genParam5, "Phase:", "0");
        hideParam(m_genLabel6, m_genParam6);
    } else if (type == "PULSE") {
        showParam(m_genLabel1, m_genParam1, "V1:", "0");
        showParam(m_genLabel2, m_genParam2, "V2:", "5");
        showParam(m_genLabel3, m_genParam3, "Delay:", "0");
        showParam(m_genLabel4, m_genParam4, "Rise:", "1u");
        showParam(m_genLabel5, m_genParam5, "Fall:", "1u");
        showParam(m_genLabel6, m_genParam6, "Width:", "500u");
    } else if (type == "PWL") {
        showParam(m_genLabel1, m_genParam1, "T1:", "0");
        showParam(m_genLabel2, m_genParam2, "V1:", "0");
        showParam(m_genLabel3, m_genParam3, "T2:", "1m");
        showParam(m_genLabel4, m_genParam4, "V2:", "5");
        showParam(m_genLabel5, m_genParam5, "T3:", "2m");
        showParam(m_genLabel6, m_genParam6, "V3:", "0");
    }
}

void SimulationPanel::onOpenPwlEditor() {
    seedDefaultPwlPointsIfNeeded();
    QDialog dlg(this);
    dlg.setWindowTitle("PWL Editor");
    QVBoxLayout* layout = new QVBoxLayout(&dlg);
    QTableWidget* table = new QTableWidget(static_cast<int>(m_pwlPoints.size()), 2, &dlg);
    table->setHorizontalHeaderLabels({"Time", "Value"});
    for (int i = 0; i < static_cast<int>(m_pwlPoints.size()); ++i) {
        table->setItem(i, 0, new QTableWidgetItem(m_pwlPoints[i].first));
        table->setItem(i, 1, new QTableWidgetItem(m_pwlPoints[i].second));
    }
    layout->addWidget(table);
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() == QDialog::Accepted) {
        m_pwlPoints.clear();
        for (int r = 0; r < table->rowCount(); ++r) {
            m_pwlPoints.push_back({table->item(r, 0)->text(), table->item(r, 1)->text()});
        }
    }
}

void SimulationPanel::onOpenStepBuilder() {
    QString currentStep;
    if (m_scene) {
        for (auto* gi : m_scene->items()) {
            auto* directive = dynamic_cast<SchematicSpiceDirectiveItem*>(gi);
            if (!directive) continue;
            if (directive->text().trimmed().startsWith(".step", Qt::CaseInsensitive)) {
                currentStep = directive->text().trimmed();
                break;
            }
        }
    }
    if (currentStep.isEmpty() && m_commandLine && m_commandLine->text().trimmed().startsWith(".step", Qt::CaseInsensitive)) {
        currentStep = m_commandLine->text().trimmed();
    }

    SpiceStepDialog dlg(currentStep, m_scene, this);
    if (dlg.exec() != QDialog::Accepted) return;

    const QString stepCommand = dlg.commandText();
    if (m_commandLine) m_commandLine->setText(stepCommand);

    if (!m_scene) return;
    SchematicSpiceDirectiveItem* found = nullptr;
    for (auto* gi : m_scene->items()) {
        auto* directive = dynamic_cast<SchematicSpiceDirectiveItem*>(gi);
        if (!directive) continue;
        if (directive->text().trimmed().startsWith(".step", Qt::CaseInsensitive)) {
            found = directive;
            break;
        }
    }

    if (found) {
        found->setText(stepCommand);
        found->update();
        return;
    }

    QPointF cmdPos(100, 200);
    if (!m_scene->views().isEmpty()) {
        if (auto* view = m_scene->views().first()) {
            cmdPos = view->mapToScene(view->viewport()->rect().center() + QPoint(120, -60));
        }
    }
    auto* cmdItem = new SchematicSpiceDirectiveItem(stepCommand, cmdPos);
    m_scene->addItem(cmdItem);
}

void SimulationPanel::onImportPwlCsv() {
    QString path = QFileDialog::getOpenFileName(this, "Import PWL CSV", m_projectDir, "CSV Files (*.csv)");
    if (!path.isEmpty()) importPwlCsvFile(path);
}

void SimulationPanel::onExportPwlCsv() {
    QString path = QFileDialog::getSaveFileName(this, "Export PWL CSV", m_projectDir, "CSV Files (*.csv)");
    if (!path.isEmpty()) exportPwlCsvFile(path);
}

void SimulationPanel::onExportResultsCsv() {
    QString path = QFileDialog::getSaveFileName(this, "Export Results CSV", m_projectDir, "CSV Files (*.csv)");
    if (!path.isEmpty()) exportResultsCsvFile(path);
}

void SimulationPanel::onExportResultsJson() {
    QString path = QFileDialog::getSaveFileName(this, "Export Results JSON", m_projectDir, "JSON Files (*.json)");
    if (!path.isEmpty()) exportResultsJsonFile(path);
}

void SimulationPanel::onExportResultsReport() {
    QString path = QFileDialog::getSaveFileName(this, "Export Results Report", m_projectDir, "Markdown Files (*.md)");
    if (!path.isEmpty()) exportResultsReportFile(path);
}

void SimulationPanel::onSaveGeneratorPreset() {
    QString name = QInputDialog::getText(this, "Save Preset", "Preset Name:");
    if (!name.isEmpty()) {
        m_userGeneratorPresets[name] = collectGeneratorConfig();
        saveUserGeneratorPresets();
        refreshGeneratorPresetCombo();
    }
}

void SimulationPanel::onDeleteGeneratorPreset() {
    QString tag = m_generatorPresetCombo->currentData().toString();
    if (tag.startsWith("U:")) {
        m_userGeneratorPresets.remove(tag.mid(2));
        saveUserGeneratorPresets();
        refreshGeneratorPresetCombo();
    }
}

void SimulationPanel::setAnalysisConfig(const AnalysisConfig& cfg) {
    if (!m_analysisType) return;

    qDebug() << "[SimulationPanel::setAnalysisConfig] type=" << static_cast<int>(cfg.type) << "fStart=" << cfg.fStart << "rfPort1Source=" << cfg.rfPort1Source;

    bool oldBuild = m_buildInProgress;
    m_buildInProgress = true;

    // Map SimAnalysisType enum to UI ComboBox indices
    int idx = 0;
    switch (cfg.type) {
        case SimAnalysisType::Transient: idx = 0; break;
        case SimAnalysisType::OP:        idx = 1; break;
        case SimAnalysisType::AC:        idx = 2; break;
        case SimAnalysisType::SParameter: idx = 3; break;
        case SimAnalysisType::MonteCarlo: idx = 4; break;
        case SimAnalysisType::ParametricSweep: idx = 5; break;
        case SimAnalysisType::Sensitivity: idx = 6; break;
        case SimAnalysisType::RealTime: idx = 7; break;
        default: {
            // Check for explicit SParameter enum value 12
            if (static_cast<int>(cfg.type) == 12) idx = 3;
            else idx = 0;
            break;
        }
    }

//    qDebug() << "[SimulationPanel::setAnalysisConfig] mapped to idx=" << idx;

    if (idx >= 0 && idx < m_analysisType->count()) {
        m_analysisType->setCurrentIndex(idx);
    }

    Q_EMIT analysisModeChanged();

    // Only set raw numeric fields if we don't have a command text to parse from.
    // This avoids losing precision/suffixes during the switch.
    if (cfg.commandText.isEmpty()) {
        if (idx == 0) {
            if (cfg.step > 0) m_param1->setText(QString::number(cfg.step, 'g', 10));
            if (cfg.stop > 0) m_param2->setText(QString::number(cfg.stop, 'g', 10));
            if (m_steadyCheck) m_steadyCheck->setChecked(cfg.transientSteady);
            if (m_steadyTolEdit) m_steadyTolEdit->setText(cfg.steadyStateTol > 0.0 ? QString::number(cfg.steadyStateTol, 'g', 12) : QString());
            if (m_steadyDelayEdit) m_steadyDelayEdit->setText(cfg.steadyStateDelay > 0.0 ? QString::number(cfg.steadyStateDelay, 'g', 12) : QString());
        } else if (idx == 2) {
            const double fStart = (cfg.fStart > 0.0) ? cfg.fStart : 10.0;
            const double fStop = (cfg.fStop > 0.0) ? cfg.fStop : 1e6;
            const int pts = (cfg.pts > 0) ? cfg.pts : 10;
            m_param1->setText(QString::number(fStart, 'g', 12));
            m_param2->setText(QString::number(fStop, 'g', 12));
            m_param3->setText(QString::number(pts));
        } else if (idx == 3) { // RF S-Parameter
            m_param1->setText(QString::number(cfg.fStart, 'g', 12));
            m_param2->setText(QString::number(cfg.fStop, 'g', 12));
            m_param3->setText(QString::number(cfg.pts));
            m_param4->setText(cfg.rfPort1Source);
            m_param5->setText(cfg.rfPort2Node);
            m_param6->setText(QString::number(cfg.rfZ0, 'g', 10));
        }
    }

    if (!cfg.commandText.isEmpty()) {
        m_commandLine->setText(cfg.commandText);
        // Use skipTypeOverride=true so that the analysisType ComboBox (just set above)
        // is not reset back by a commandText like ".ac ..." when SParameter is selected.
        const bool isSParameter = (idx == 3);
        parseCommandText(cfg.commandText, isSParameter);
    } else {
        updateCommandDisplay();
    }
    m_buildInProgress = oldBuild;
}

SimulationPanel::AnalysisConfig SimulationPanel::getAnalysisConfig() const {
    AnalysisConfig cfg{};
    const int idx = m_analysisType ? m_analysisType->currentIndex() : 0;
    if (idx == 0) {
        cfg.type = SimAnalysisType::Transient;
        cfg.step = parseValue(m_param1 ? m_param1->text() : QString(), 1e-6);
        cfg.stop = parseValue(m_param2 ? m_param2->text() : QString(), 10e-3);
        cfg.transientSteady = m_steadyCheck && m_steadyCheck->isChecked();
        cfg.steadyStateTol = parseValue(m_steadyTolEdit ? m_steadyTolEdit->text() : QString(), 0.0);
        cfg.steadyStateDelay = parseValue(m_steadyDelayEdit ? m_steadyDelayEdit->text() : QString(), 0.0);
    } else if (idx == 1) {
        cfg.type = SimAnalysisType::OP;
    } else if (idx == 2) {
        cfg.type = SimAnalysisType::AC;
        cfg.fStart = parseValue(m_param1 ? m_param1->text() : QString(), 10);
        cfg.fStop = parseValue(m_param2 ? m_param2->text() : QString(), 1e6);
        cfg.pts = std::max(1, (m_param3 ? m_param3->text().trimmed().toInt() : 10));
    } else if (idx == 3) {
        cfg.type = SimAnalysisType::SParameter;
        cfg.fStart = parseValue(m_param1 ? m_param1->text() : QString(), 10);
        cfg.fStop = parseValue(m_param2 ? m_param2->text() : QString(), 1e6);
        cfg.pts = std::max(1, (m_param3 ? m_param3->text().trimmed().toInt() : 10));
        cfg.rfPort1Source = m_param4 ? m_param4->text().trimmed() : "V1";
        cfg.rfPort2Node = m_param5 ? m_param5->text().trimmed() : "OUT";
        cfg.rfZ0 = parseValue(m_param6 ? m_param6->text().trimmed() : QString(), 50.0);
    } else if (idx == 4) {
        cfg.type = SimAnalysisType::MonteCarlo;
    } else if (idx == 5) {
        cfg.type = SimAnalysisType::ParametricSweep;
    } else if (idx == 6) {
        cfg.type = SimAnalysisType::Sensitivity;
    } else if (idx == 7) {
        cfg.type = SimAnalysisType::RealTime;
    }
    
    cfg.commandText = m_commandLine ? m_commandLine->text() : QString();
    return cfg;
}

bool SimulationPanel::isRealTimeMode() const {
    return m_analysisType && m_analysisType->currentIndex() == 7;
}

void SimulationPanel::setSchematicName(const QString& name) {
    if (m_schematicNameLabel) {
        if (name.isEmpty()) {
            m_schematicNameLabel->setText("SOURCE: Active Tab");
        } else {
            m_schematicNameLabel->setText("SOURCE: " + name);
        }
    }
    
    if (m_waveformViewer) {
        m_waveformViewer->setSourceSchematicName(name);
    }
}


