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
#include <QStackedWidget>
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
    
    setObjectName("SimulationPanel");
    PCBTheme* theme = ThemeManager::theme();
    QString bg = theme ? theme->windowBackground().name() : "#1e1e1e";
    setStyleSheet(QString("#SimulationPanel { background-color: %1; }").arg(bg));

    createToolbar(mainLayout);

    QSplitter* mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainSplitter->setHandleWidth(2);
    QString borderColor = theme ? theme->panelBorder().name() : "#3c3c3c";
    mainSplitter->setStyleSheet(QString("QSplitter::handle { background-color: %1; }").arg(borderColor));

    createSidebar(mainSplitter);
    createMainView(mainSplitter);

    mainSplitter->setSizes({280, 600});
    mainLayout->addWidget(mainSplitter);

    onAnalysisChanged(m_analysisType->currentIndex());
    onGeneratorTypeChanged(m_generatorType->currentIndex());
    loadGeneratorLibrary();
    applyPlotQuality();
    updateCommandDisplay();
}

void SimulationPanel::createToolbar(QVBoxLayout* layout) {
    PCBTheme* theme = ThemeManager::theme();
    QString bg = theme ? theme->windowBackground().name() : "#1e1e1e";
    QString panelBg = theme ? theme->panelBackground().name() : "#252526";
    QString textColor = theme ? theme->textColor().name() : "#cccccc";
    QString accent = theme ? theme->accentColor().name() : "#3b82f6";
    QString borderColor = theme ? theme->panelBorder().name() : "#3c3c3c";
    const bool isLight = theme && theme->type() == PCBTheme::Light;

    auto buttonStyle = [&](const QString& bgColor, const QString& fgColor) {
        return QString("background-color: %1; color: %2; font-weight: bold; padding: 4px 10px; border-radius: 4px; border: 1px solid %3;")
            .arg(bgColor, fgColor, borderColor);
    };
    const QString checkboxStyle = QString("QCheckBox { color: %1; font-weight: bold; }").arg(textColor);

    QToolBar* toolbar = new QToolBar("Simulation Controls");
    toolbar->setIconSize(QSize(20, 20));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->setStyleSheet(QString(
        "QToolBar { background-color: %1; border-bottom: 1px solid %2; padding: 4px; spacing: 8px; }"
        "QToolButton { background: %3; border: 1px solid %2; border-radius: 2px; padding: 4px 8px; font-weight: bold; color: %4; }"
        "QToolButton:hover { background: %5; }"
    ).arg(panelBg, borderColor, bg, textColor, accent));

    m_runButton = new QPushButton("Run");
    m_runButton->setStyleSheet(buttonStyle("#065f46", "white"));
    connect(m_runButton, &QPushButton::clicked, this, &SimulationPanel::onRunSimulation);
    toolbar->addWidget(m_runButton);

    QPushButton* exportReportBtn = new QPushButton("Report");
    exportReportBtn->setStyleSheet(buttonStyle("#0f766e", "white"));
    connect(exportReportBtn, &QPushButton::clicked, this, &SimulationPanel::onExportResultsReport);
    toolbar->addWidget(exportReportBtn);

    m_overlayPreviousRun = new QCheckBox("Overlay");
    m_overlayPreviousRun->setStyleSheet(checkboxStyle);
    connect(m_overlayPreviousRun, &QCheckBox::toggled, this, [this](bool) {
        if (m_hasLastResults) plotBuiltinResults(m_lastResults);
    });
    toolbar->addWidget(m_overlayPreviousRun);

    QPushButton* probePinBtn = new QPushButton("Probe Canvas");
    probePinBtn->setStyleSheet(buttonStyle("#1d4ed8", "white"));
    connect(probePinBtn, &QPushButton::clicked, this, [this]() { Q_EMIT placementToolRequested("Probe"); });
    toolbar->addWidget(probePinBtn);

    QPushButton* clearProbesBtn = new QPushButton("Clear Probes");
    clearProbesBtn->setStyleSheet(buttonStyle(isLight ? "#cbd5e1" : "#4b5563", isLight ? textColor : "white"));
    connect(clearProbesBtn, &QPushButton::clicked, this, &SimulationPanel::clearAllProbes);
    toolbar->addWidget(clearProbesBtn);

    toolbar->addSeparator();

    QCheckBox* showVoltageCheck = new QCheckBox("V");
    showVoltageCheck->setChecked(true);
    showVoltageCheck->setStyleSheet(checkboxStyle);
    toolbar->addWidget(showVoltageCheck);

    QCheckBox* showCurrentCheck = new QCheckBox("I");
    showCurrentCheck->setChecked(true);
    showCurrentCheck->setStyleSheet(checkboxStyle);
    toolbar->addWidget(showCurrentCheck);

    auto updateOverlays = [this, showVoltageCheck, showCurrentCheck]() {
        Q_EMIT overlayVisibilityChanged(showVoltageCheck->isChecked(), showCurrentCheck->isChecked());
    };
    connect(showVoltageCheck, &QCheckBox::toggled, this, updateOverlays);
    connect(showCurrentCheck, &QCheckBox::toggled, this, updateOverlays);

    QPushButton* netlistBtn = new QPushButton("Netlist");
    connect(netlistBtn, &QPushButton::clicked, this, &SimulationPanel::onViewNetlist);
    toolbar->addWidget(netlistBtn);

    layout->addWidget(toolbar);
}

void SimulationPanel::createSidebar(QSplitter* splitter) {
    PCBTheme* theme = ThemeManager::theme();
    QString panelBg = theme ? theme->panelBackground().name() : "#252526";
    QString borderColor = theme ? theme->panelBorder().name() : "#3c3c3c";
    QString accent = theme ? theme->accentColor().name() : "#3b82f6";
    QString bg = theme ? theme->windowBackground().name() : "#1e1e1e";

    QWidget* sidebar = new QWidget();
    sidebar->setMinimumWidth(240);
    sidebar->setStyleSheet(QString("QWidget { background-color: %1; }").arg(panelBg));
    QVBoxLayout* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(8, 8, 8, 8);
    sidebarLayout->setSpacing(10);

    m_schematicNameLabel = new QLabel("SOURCE: None");
    m_schematicNameLabel->setStyleSheet(QString("QLabel { font-weight: bold; font-size: 11px; color: %1; background-color: %2; padding: 6px; border-radius: 4px; border: 1px solid %3; }")
                                           .arg(accent, bg, borderColor));
    m_schematicNameLabel->setAlignment(Qt::AlignCenter);
    sidebarLayout->addWidget(m_schematicNameLabel);

    sidebarLayout->addWidget(createAnalysisSetupWidget());
    sidebarLayout->addWidget(createMonitorWidget());

    m_logOutput = new QTextEdit();
    m_logOutput->setReadOnly(true);
    m_logOutput->setMaximumHeight(80);
    m_logOutput->setStyleSheet(QString("QTextEdit { background: %1; border: 1px solid %2; font-family: monospace; font-size: 9px; color: #eee; }").arg(bg, borderColor));
    sidebarLayout->addWidget(m_logOutput);

    QPushButton* showFullLogBtn = new QPushButton("Show Detailed Log");
    showFullLogBtn->setStyleSheet("QPushButton { background: #374151; color: white; border-radius: 3px; padding: 4px; font-size: 10px; }");
    connect(showFullLogBtn, &QPushButton::clicked, this, &SimulationPanel::showDetailedLog);
    sidebarLayout->addWidget(showFullLogBtn);

    QScrollArea* sidebarScroll = new QScrollArea();
    sidebarScroll->setWidgetResizable(true);
    sidebarScroll->setFrameShape(QFrame::NoFrame);
    sidebarScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sidebarScroll->setStyleSheet(QString("QScrollArea { background-color: %1; border-right: 1px solid %2; }").arg(panelBg, borderColor));
    sidebarScroll->setWidget(sidebar);
    splitter->addWidget(sidebarScroll);
}

QWidget* SimulationPanel::createAnalysisSetupWidget() {
    PCBTheme* theme = ThemeManager::theme();
    QString bg = theme ? theme->windowBackground().name() : "#1e1e1e";
    QString borderColor = theme ? theme->panelBorder().name() : "#3c3c3c";
    QString textColor = theme ? theme->textColor().name() : "#cccccc";
    QString accent = theme ? theme->accentColor().name() : "#3b82f6";
    const bool isLight = theme && theme->type() == PCBTheme::Light;
    const QString inputBg = isLight ? "#ffffff" : "#121214";
    const QString inputStyle = QString("QLineEdit, QComboBox, QDoubleSpinBox { background: %1; color: %2; border: 1px solid %3; }")
        .arg(inputBg, textColor, borderColor);
    const QString commandStyle = QString("QLineEdit { background: %1; color: %2; border: 1px solid %2; font-family: 'Courier New'; font-weight: bold; }")
        .arg(isLight ? "#eff6ff" : "#1e3a5f", accent);
    auto buttonStyle = [&](const QString& bgColor, const QString& fgColor) {
        return QString("background-color: %1; color: %2; font-weight: bold; padding: 4px 10px; border-radius: 4px; border: 1px solid %3;")
            .arg(bgColor, fgColor, borderColor);
    };

    QGroupBox* group = new QGroupBox("SIMULATION DASHBOARD");
    group->setStyleSheet(QString("QGroupBox { font-weight: bold; color: %1; border: 1px solid %2; margin-top: 10px; padding-top: 10px; } "
                                 "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; }")
                         .arg(accent, borderColor));
    QFormLayout* layout = new QFormLayout(group);

    // m_analysisType is now internal-only to avoid conflicting with schematic settings
    m_analysisType = new QComboBox();
    m_analysisType->addItems({"Auto-Detect", "Transient", "DC OP", "DC Sweep", "AC Sweep", "RF S-Parameter", "Monte Carlo", "Parametric Sweep", "Sensitivity", "Real-time Mode"});
    m_analysisType->setStyleSheet(inputStyle);
    // layout->addRow("Mode:", m_analysisType); // Removed manual override
    
    m_commandLine = new QLineEdit();
    m_commandLine->setReadOnly(true);
    m_commandLine->setPlaceholderText("Schematic directive will be used...");
    m_commandLine->setStyleSheet(commandStyle);
    layout->addRow("Active:", m_commandLine);

    m_acSweepType = new QComboBox();
    m_acSweepType->addItems({"Decade", "Octave", "Linear"});
    m_acSweepType->setStyleSheet(inputStyle);
    m_acSweepType->setVisible(false);

    m_param1 = new QLineEdit("1u");
    m_param2 = new QLineEdit("10m");
    m_param3 = new QLineEdit("0");
    m_param4 = new QLineEdit();
    m_param5 = new QLineEdit();
    m_param6 = new QLineEdit("50");
    
    m_steadyCheck = new QCheckBox("Steady State");
    m_steadyTolEdit = new QLineEdit();
    m_steadyDelayEdit = new QLineEdit();
    
    // Add but hide manual overrides (driven by schematic or manual mode)
    /* Removed manual overrides to keep schematic as source of truth
    layout->addRow("P1:", m_param1);
    layout->addRow("P2:", m_param2);
    layout->addRow("P3:", m_param3);
    layout->addRow("P4:", m_param4);
    layout->addRow("P5:", m_param5);
    layout->addRow("P6:", m_param6);
    layout->addRow("Sweep:", m_acSweepType);
    layout->addRow("", m_steadyCheck);
    layout->addRow("Tol:", m_steadyTolEdit);
    layout->addRow("Delay:", m_steadyDelayEdit);
    */

    // Hide all manual overrides by default (index 0: Auto-Detect)
    for(auto* w : { (QWidget*)m_param1, (QWidget*)m_param2, (QWidget*)m_param3, (QWidget*)m_param4, (QWidget*)m_param5, (QWidget*)m_param6, (QWidget*)m_acSweepType, (QWidget*)m_steadyCheck, (QWidget*)m_steadyTolEdit, (QWidget*)m_steadyDelayEdit }) {
        w->setVisible(false);
        if (auto* lbl = layout->labelForField(w)) lbl->setVisible(false);
    }


    connect(m_analysisType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SimulationPanel::onAnalysisChanged);
    connect(m_analysisType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SimulationPanel::updateSchematicDirective);
    connect(m_analysisType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SimulationPanel::updateCommandDisplay);
    connect(m_param1, &QLineEdit::textChanged, this, &SimulationPanel::updateSchematicDirective);
    connect(m_param1, &QLineEdit::textChanged, this, &SimulationPanel::updateCommandDisplay);
    connect(m_param2, &QLineEdit::textChanged, this, &SimulationPanel::updateSchematicDirective);
    connect(m_param2, &QLineEdit::textChanged, this, &SimulationPanel::updateCommandDisplay);
    connect(m_param3, &QLineEdit::textChanged, this, &SimulationPanel::updateSchematicDirective);
    connect(m_param3, &QLineEdit::textChanged, this, &SimulationPanel::updateCommandDisplay);
    connect(m_param4, &QLineEdit::textChanged, this, &SimulationPanel::updateSchematicDirective);
    connect(m_param4, &QLineEdit::textChanged, this, &SimulationPanel::updateCommandDisplay);
    connect(m_param5, &QLineEdit::textChanged, this, &SimulationPanel::updateSchematicDirective);
    connect(m_param5, &QLineEdit::textChanged, this, &SimulationPanel::updateCommandDisplay);
    connect(m_param6, &QLineEdit::textChanged, this, &SimulationPanel::updateSchematicDirective);
    connect(m_param6, &QLineEdit::textChanged, this, &SimulationPanel::updateCommandDisplay);
    connect(m_acSweepType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SimulationPanel::updateSchematicDirective);
    connect(m_acSweepType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SimulationPanel::updateCommandDisplay);
    connect(m_steadyCheck, &QCheckBox::toggled, this, &SimulationPanel::updateSchematicDirective);
    connect(m_steadyCheck, &QCheckBox::toggled, this, &SimulationPanel::updateCommandDisplay);
    connect(m_commandLine, &QLineEdit::editingFinished, this, [this]() { parseCommandText(m_commandLine->text()); });

    return group;
}

QWidget* SimulationPanel::createMonitorWidget() {
    PCBTheme* theme = ThemeManager::theme();
    QString bg = theme ? theme->windowBackground().name() : "#1e1e1e";
    QString borderColor = theme ? theme->panelBorder().name() : "#3c3c3c";
    QString accent = theme ? theme->accentColor().name() : "#3b82f6";
    QString textColor = theme ? theme->textColor().name() : "#cccccc";

    QWidget* monitor = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(monitor);
    layout->setContentsMargins(0, 0, 0, 0);

    QLabel* title = new QLabel("TRACE MONITOR");
    title->setStyleSheet(QString("font-weight: bold; font-size: 10px; color: %1;").arg(accent));
    layout->addWidget(title);

    m_signalList = new QListWidget();
    m_signalList->setStyleSheet(QString(
        "QListWidget { background: %1; border: 1px solid %2; border-radius: 3px; color: #eee; }"
        "QListWidget::item { padding: 4px; border-bottom: 1px solid %2; }"
        "QListWidget::item:selected { background: %3; color: white; }"
    ).arg(bg, borderColor, accent));
    m_signalList->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_signalList, 2);

    connect(m_signalList, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        QString seriesName = item->text();
        bool isVisible = (item->checkState() == Qt::Checked);
        if (m_waveformViewer) {
            m_waveformViewer->setSignalChecked(seriesName, isVisible);
            m_waveformViewer->updatePlot(false);
        }
    });
    connect(m_signalList, &QListWidget::customContextMenuRequested, this, &SimulationPanel::onSignalListContextMenuRequested);

    m_issueList = new QListWidget();
    m_issueList->setStyleSheet(QString("QListWidget { background: %1; border: 1px solid %2; color: #fbbf24; font-size: 9px; }").arg(bg, borderColor));
    
    layout->addWidget(new QLabel("SIM ISSUES"), 0, Qt::AlignBottom);
    layout->addWidget(m_issueList, 1);

    return monitor;
}

void SimulationPanel::createMainView(QSplitter* splitter) {
    PCBTheme* theme = ThemeManager::theme();
    QString chartBg = theme && theme->type() == PCBTheme::Light ? "#ffffff" : "#000000";
    QString chartPlotBg = theme && theme->type() == PCBTheme::Light ? "#f8fafc" : "#000000";
    QString textColor = theme ? theme->textColor().name() : "#cccccc";
    QString accent = theme ? theme->accentColor().name() : "#3b82f6";
    QString borderColor = theme ? theme->panelBorder().name() : "#3c3c3c";
    QString panelBg = theme ? theme->panelBackground().name() : "#252526";

    QWidget* container = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);

    m_viewTabs = new QTabWidget();
    m_viewTabs->setStyleSheet(QString("QTabWidget::pane { border: 1px solid %1; } QTabBar::tab { background: %2; color: %3; padding: 6px 12px; } QTabBar::tab:selected { background: %4; color: white; }")
                            .arg(borderColor, panelBg, textColor, accent));

    m_chart = new QChart();
    m_chart->setBackgroundBrush(QBrush(QColor(chartBg)));
    m_chart->setPlotAreaBackgroundBrush(QBrush(QColor(chartPlotBg)));
    m_chart->setPlotAreaBackgroundVisible(true);
    m_plotView = new QChartView(m_chart);
    m_plotView->setStyleSheet(QString("border: none; background: %1;").arg(chartBg));
    
    m_waveformViewer = new WaveformViewer();
    
    QStackedWidget* wavesStack = new QStackedWidget();
    wavesStack->addWidget(m_waveformViewer);
    wavesStack->addWidget(m_plotView);
    
    m_viewTabs->addTab(wavesStack, "Waves");

    m_spectrumChart = new QChart();
    m_spectrumChart->setBackgroundBrush(QBrush(QColor(chartBg)));
    m_spectrumChart->setPlotAreaBackgroundBrush(QBrush(QColor(chartPlotBg)));
    m_spectrumChart->setPlotAreaBackgroundVisible(true);
    m_spectrumView = new QChartView(m_spectrumChart);
    m_spectrumTab = new QWidget();
    QVBoxLayout* specLay = new QVBoxLayout(m_spectrumTab);
    
    QHBoxLayout* specHeader = new QHBoxLayout();
    m_steppedMeasSeriesCombo = new QComboBox();
    m_steppedMeasAxisCombo = new QComboBox();
    m_steppedMeasSeriesCombo->setStyleSheet(QString("QComboBox { background: %1; color: %2; }").arg(chartBg, textColor));
    m_steppedMeasAxisCombo->setStyleSheet(QString("QComboBox { background: %1; color: %2; }").arg(chartBg, textColor));
    specHeader->addWidget(new QLabel("Stepped:"));
    specHeader->addWidget(m_steppedMeasSeriesCombo);
    specHeader->addWidget(new QLabel("Axis:"));
    specHeader->addWidget(m_steppedMeasAxisCombo);
    specLay->addLayout(specHeader);
    
    specLay->addWidget(m_spectrumView);
    m_viewTabs->addTab(m_spectrumTab, "Spectrum");

    m_rfTab = new QWidget();
    QVBoxLayout* rfLay = new QVBoxLayout(m_rfTab);
    m_smithChart = new SmithChartWidget();
    rfLay->addWidget(m_smithChart);
    m_viewTabs->addTab(m_rfTab, "RF Analysis");

    m_generatorTab = createGeneratorWidget();
    // m_viewTabs->addTab(m_generatorTab, "Generators"); // Removed redundant generator setup tab

    QWidget* measTab = createMeasurementsWidget();
    m_viewTabs->addTab(measTab, "Measurements");

    m_efficiencyTab = new QWidget();
    QVBoxLayout* effLay = new QVBoxLayout(m_efficiencyTab);
    
    m_efficiencySummaryLabel = new QLabel("Run simulation to see efficiency analysis.");
    m_efficiencySummaryLabel->setStyleSheet(QString("color: %1; font-weight: bold;").arg(textColor));
    effLay->addWidget(m_efficiencySummaryLabel);
    
    m_efficiencyTable = new QTableWidget(0, 2);
    m_efficiencyTable->setHorizontalHeaderLabels({"Metric", "Value"});
    m_efficiencyTable->setStyleSheet(QString("QTableWidget { background: %1; color: %2; }").arg(chartBg, textColor));
    effLay->addWidget(m_efficiencyTable);
    m_viewTabs->addTab(m_efficiencyTab, "Efficiency");

    m_designExplorerTab = new QWidget();
    QVBoxLayout* explorerLay = new QVBoxLayout(m_designExplorerTab);
    
    m_designExplorerSummaryLabel = new QLabel("Run sweep or optimization to explore design space.");
    m_designExplorerSummaryLabel->setStyleSheet(QString("color: %1; font-weight: bold;").arg(textColor));
    explorerLay->addWidget(m_designExplorerSummaryLabel);
    
    m_designExplorerTable = new QTableWidget(0, 5);
    m_designExplorerTable->setStyleSheet(QString("QTableWidget { background: %1; color: %2; }").arg(chartBg, textColor));
    explorerLay->addWidget(m_designExplorerTable);
    
    m_designExplorerDetailLabel = new QLabel("Select a case for details.");
    m_designExplorerDetailLabel->setWordWrap(true);
    m_designExplorerDetailLabel->setStyleSheet(QString("color: %1;").arg(textColor));
    explorerLay->addWidget(m_designExplorerDetailLabel);
    
    m_designExplorerCopyButton = new QPushButton("Copy Configuration");
    m_designExplorerCopyButton->setEnabled(false);
    explorerLay->addWidget(m_designExplorerCopyButton);
    
    m_viewTabs->addTab(m_designExplorerTab, "Explorer");

    m_scopeContainer = wavesStack; // Link only the waveform stack to remove tabs in dock mode
    layout->addWidget(m_viewTabs);
    
    // Timeline / Time-Travel Control
    QWidget* timelineContainer = new QWidget();
    QHBoxLayout* timelineLay = new QHBoxLayout(timelineContainer);
    timelineLay->setContentsMargins(8, 10, 8, 10);
    
    m_timelineIcon = new QLabel("🕒");
    timelineLay->addWidget(m_timelineIcon);
    
    m_timelineSlider = new QSlider(Qt::Horizontal);
    m_timelineSlider->setRange(0, 1000);
    m_timelineSlider->setEnabled(false);
    m_timelineSlider->setMinimumHeight(32); // Increased to prevent clipping
    m_timelineSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_timelineSlider->setStyleSheet(QString(
        "QSlider::groove:horizontal { border: 1px solid %1; height: 4px; background: %2; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: %3; border: 1px solid %3; width: 14px; margin: -5px 0; border-radius: 7px; }"
    ).arg(borderColor, panelBg, accent));
    timelineLay->addWidget(m_timelineSlider);
    
    m_timelineLabel = new QLabel("--- s");
    m_timelineLabel->setFixedWidth(100);
    m_timelineLabel->setStyleSheet(QString("font-family: monospace; color: %1;").arg(textColor));
    timelineLay->addWidget(m_timelineLabel);
    
    layout->addWidget(timelineContainer);

    splitter->addWidget(container);

    connect(m_viewTabs, &QTabWidget::currentChanged, this, [this](int) {
        if (m_hasLastResults) plotBuiltinResults(m_lastResults);
    });
    
    connect(m_timelineSlider, &QSlider::valueChanged, this, &SimulationPanel::onTimelineValueChanged);
}

QWidget* SimulationPanel::createGeneratorWidget() {
    PCBTheme* theme = ThemeManager::theme();
    QString textColor = theme ? theme->textColor().name() : "#cccccc";
    QString borderColor = theme ? theme->panelBorder().name() : "#3c3c3c";
    const bool isLight = theme && theme->type() == PCBTheme::Light;
    const QString inputBg = isLight ? "#ffffff" : "#121214";
    const QString inputStyle = QString("QLineEdit, QComboBox { background: %1; color: %2; border: 1px solid %3; padding: 2px; }")
        .arg(inputBg, textColor, borderColor);

    QWidget* widget = new QWidget();
    QVBoxLayout* mainLay = new QVBoxLayout(widget);

    QGroupBox* group = new QGroupBox("SOURCE GENERATOR CONFIGURATION");
    group->setStyleSheet("QGroupBox { font-weight: bold; }");
    QFormLayout* layout = new QFormLayout(group);

    m_generatorType = new QComboBox();
    m_generatorType->addItems({"DC", "SIN", "PULSE", "EXP", "SFFM", "PWL", "AM", "FM"});
    m_generatorType->setStyleSheet(inputStyle);
    layout->addRow("Type:", m_generatorType);

    m_generatorPresetCombo = new QComboBox();
    m_generatorPresetCombo->setStyleSheet(inputStyle);
    layout->addRow("Template:", m_generatorPresetCombo);

    m_genLabel1 = new QLabel("P1:"); m_genParam1 = new QLineEdit("5");
    m_genLabel2 = new QLabel("P2:"); m_genParam2 = new QLineEdit("1");
    m_genLabel3 = new QLabel("P3:"); m_genParam3 = new QLineEdit("1k");
    m_genLabel4 = new QLabel("P4:"); m_genParam4 = new QLineEdit("0");
    m_genLabel5 = new QLabel("P5:"); m_genParam5 = new QLineEdit("0");
    m_genLabel6 = new QLabel("P6:"); m_genParam6 = new QLineEdit("0");

    for (auto* l : {m_genParam1, m_genParam2, m_genParam3, m_genParam4, m_genParam5, m_genParam6}) l->setStyleSheet(inputStyle);

    layout->addRow(m_genLabel1, m_genParam1);
    layout->addRow(m_genLabel2, m_genParam2);
    layout->addRow(m_genLabel3, m_genParam3);
    layout->addRow(m_genLabel4, m_genParam4);
    layout->addRow(m_genLabel5, m_genParam5);
    layout->addRow(m_genLabel6, m_genParam6);

    QPushButton* applyBtn = new QPushButton("Apply to Selected Source");
    applyBtn->setStyleSheet("background: #0f766e; color: white; font-weight: bold; padding: 8px;");
    connect(applyBtn, &QPushButton::clicked, this, &SimulationPanel::onApplyGeneratorToSelection);
    layout->addRow("", applyBtn);

    mainLay->addWidget(group);
    mainLay->addStretch();

    connect(m_generatorType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SimulationPanel::onGeneratorTypeChanged);
    connect(m_generatorPresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SimulationPanel::onGeneratorPresetActivated);

    return widget;
}

QWidget* SimulationPanel::createMeasurementsWidget() {
    PCBTheme* theme = ThemeManager::theme();
    QString bg = theme ? theme->windowBackground().name() : "#1e1e1e";
    QString textColor = theme ? theme->textColor().name() : "#cccccc";
    QString borderColor = theme ? theme->panelBorder().name() : "#3c3c3c";

    QWidget* widget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(widget);

    m_measurementsTable = new QTableWidget(0, 6);
    m_measurementsTable->setHorizontalHeaderLabels({"Name", "Result", "Avg", "RMS", "Freq", "Delta"});
    m_measurementsTable->setStyleSheet(QString("QTableWidget { background: %1; color: %2; border: 1px solid %3; }").arg(bg, textColor, borderColor));
    m_measurementsTable->horizontalHeader()->setStretchLastSection(true);
    m_measurementsTable->verticalHeader()->hide();
    
    layout->addWidget(m_measurementsTable);
    return widget;
}


void SimulationPanel::onViewNetlist() {
    updateSchematicDirective();
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
    if (m_waveformViewer) {
        m_waveformViewer->setAcMode(index == 4 || index == 5); // AC or S-Parameter
    }
    Q_EMIT analysisModeChanged();

    if (!m_param1 || !m_param1->parentWidget()) return;
    QFormLayout* layout = qobject_cast<QFormLayout*>(m_param1->parentWidget()->layout());
    if (!layout) return;
    
    auto setLabel = [&](QWidget* field, const QString& text) {
        if (!field) return;
        field->setVisible(true);
        if (auto* lbl = qobject_cast<QLabel*>(layout->labelForField(field))) {
            lbl->setText(text);
            lbl->setVisible(true);
        }
    };

    auto hideAll = [&]() {
        for(auto* w : { (QWidget*)m_param1, (QWidget*)m_param2, (QWidget*)m_param3, (QWidget*)m_param4, (QWidget*)m_param5, (QWidget*)m_param6, (QWidget*)m_acSweepType, (QWidget*)m_steadyCheck, (QWidget*)m_steadyTolEdit, (QWidget*)m_steadyDelayEdit }) {
            if (w) {
                w->setVisible(false);
                if (auto* lbl = layout->labelForField(w)) lbl->setVisible(false);
            }
        }
    };

    hideAll();

    if (index == 0) { // Auto-Detect
        if (m_viewTabs) m_viewTabs->setCurrentIndex(0); // Waves
    } else if (index == 1) { // Transient
        setLabel(m_param1, "Step:");
        setLabel(m_param2, "Stop Time:");
        setLabel(m_param3, "Start Time:");
        setLabel(m_steadyCheck, "Use Steady State:");
        setLabel(m_steadyTolEdit, "ssTol:");
        setLabel(m_steadyDelayEdit, "ssDelay:");
        if (m_viewTabs) m_viewTabs->setCurrentIndex(0);
    } else if (index == 2) { // DC OP
        if (m_viewTabs) m_viewTabs->setCurrentIndex(0);
    } else if (index == 3) { // DC Sweep
        setLabel(m_param1, "Start:");
        setLabel(m_param2, "Stop:");
        setLabel(m_param3, "Step:");
        setLabel(m_param4, "Source:");
        if (m_viewTabs) m_viewTabs->setCurrentIndex(0);
    } else if (index == 4) { // AC Sweep
        setLabel(m_param1, "Start Freq:");
        setLabel(m_param2, "Stop Freq:");
        setLabel(m_param3, "Points:");
        setLabel(m_param4, "Source:");
        setLabel(m_acSweepType, "Sweep Type:");
        if (m_viewTabs) m_viewTabs->setCurrentIndex(0);
    } else if (index == 5) { // RF S-Parameter
        setLabel(m_param1, "Start Freq:");
        setLabel(m_param2, "Stop Freq:");
        setLabel(m_param3, "Points:");
        setLabel(m_param4, "Port 1 Src:");
        setLabel(m_param5, "Port 2 Node:");
        setLabel(m_param6, "Z0:");
        setLabel(m_acSweepType, "Sweep Type:");
        if (m_viewTabs) m_viewTabs->setCurrentIndex(2); // RF Analysis tab
    } else if (index == 6) { // Parametric Sweep
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
    } else if (index == 7) { // Sensitivity
        setLabel(m_param1, "Target Signal:");
        m_param1->setVisible(true); m_param2->setVisible(false); m_param3->setVisible(false);
        m_steadyCheck->setVisible(false); m_steadyTolEdit->setVisible(false); m_steadyDelayEdit->setVisible(false);
        m_param4->setVisible(false); m_param5->setVisible(false); m_param6->setVisible(false);
        m_param1->setText("V(Out)");
    } else if (index == 8) { // Real-time
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
    int idx = 0; // Default to Auto-Detect
    switch (cfg.type) {
        case SimAnalysisType::Transient:      idx = 1; break;
        case SimAnalysisType::OP:             idx = 2; break;
        case SimAnalysisType::DC:             idx = 3; break;
        case SimAnalysisType::AC:             idx = 4; break;
        case SimAnalysisType::SParameter:     idx = 5; break;
        case SimAnalysisType::MonteCarlo:     idx = 6; break;
        case SimAnalysisType::ParametricSweep: idx = 7; break;
        case SimAnalysisType::Sensitivity:     idx = 8; break;
        case SimAnalysisType::RealTime:        idx = 9; break;
        default: {
            // Check for explicit SParameter enum value 12
            if (static_cast<int>(cfg.type) == 12) idx = 5;
            else idx = 0; // Auto-Detect
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
        } else if (idx == 2) { // DC Sweep
            m_param1->setText(QString::number(cfg.fStart, 'g', 12));
            m_param2->setText(QString::number(cfg.fStop, 'g', 12));
            m_param3->setText(QString::number(cfg.step, 'g', 12));
            m_param4->setText(cfg.dcSource);
        } else if (idx == 3) { // AC Sweep
            const double fStart = (cfg.fStart > 0.0) ? cfg.fStart : 10.0;
            const double fStop = (cfg.fStop > 0.0) ? cfg.fStop : 1e6;
            const int pts = (cfg.pts > 0) ? cfg.pts : 10;
            m_param1->setText(QString::number(fStart, 'g', 12));
            m_param2->setText(QString::number(fStop, 'g', 12));
            m_param3->setText(QString::number(pts));
            if (m_acSweepType) m_acSweepType->setCurrentIndex(static_cast<int>(cfg.acSweepType));
        } else if (idx == 4) { // RF S-Parameter
            m_param1->setText(QString::number(cfg.fStart, 'g', 12));
            m_param2->setText(QString::number(cfg.fStop, 'g', 12));
            m_param3->setText(QString::number(cfg.pts));
            m_param4->setText(cfg.rfPort1Source);
            m_param5->setText(cfg.rfPort2Node);
            m_param6->setText(QString::number(cfg.rfZ0, 'g', 12));
            if (m_acSweepType) m_acSweepType->setCurrentIndex(static_cast<int>(cfg.acSweepType));
        }
    }

    if (!cfg.commandText.isEmpty()) {
        m_commandLine->setText(cfg.commandText);
        // Use skipTypeOverride=true so that the analysisType ComboBox (just set above)
        // is not reset back by a commandText like ".ac ..." when SParameter is selected.
        const bool isSParameter = (idx == 4);
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
        // Auto-Detect: We can't know the exact type without parsing the command line or scene,
        // but for config purposes, we might want to return the last detected type.
        // For now, let's look at the command text.
        if (m_commandLine && m_commandLine->text().trimmed().startsWith(".op", Qt::CaseInsensitive)) {
            cfg.type = SimAnalysisType::OP;
        } else if (m_commandLine && m_commandLine->text().trimmed().startsWith(".ac", Qt::CaseInsensitive)) {
            cfg.type = SimAnalysisType::AC;
        } else if (m_commandLine && m_commandLine->text().trimmed().startsWith(".dc", Qt::CaseInsensitive)) {
            cfg.type = SimAnalysisType::DC;
        } else {
            cfg.type = SimAnalysisType::Transient; // Fallback
        }
    } else if (idx == 1) {
        cfg.type = SimAnalysisType::Transient;
    } else if (idx == 2) {
        cfg.type = SimAnalysisType::OP;
    } else if (idx == 3) {
        cfg.type = SimAnalysisType::DC;
        cfg.fStart = parseValue(m_param1 ? m_param1->text() : QString(), 0);
        cfg.fStop = parseValue(m_param2 ? m_param2->text() : QString(), 5);
        cfg.step = parseValue(m_param3 ? m_param3->text() : QString(), 0.1);
        cfg.dcSource = m_param4 ? m_param4->text().trimmed() : "V1";
    } else if (idx == 4) {
        cfg.type = SimAnalysisType::AC;
        cfg.fStart = parseValue(m_param1 ? m_param1->text() : QString(), 10);
        cfg.fStop = parseValue(m_param2 ? m_param2->text() : QString(), 1e6);
        cfg.pts = std::max(1, (m_param3 ? m_param3->text().trimmed().toInt() : 10));
        if (m_acSweepType) cfg.acSweepType = static_cast<SimAcSweepType>(m_acSweepType->currentIndex());
    } else if (idx == 5) {
        cfg.type = SimAnalysisType::SParameter;
        cfg.fStart = parseValue(m_param1 ? m_param1->text() : QString(), 10);
        cfg.fStop = parseValue(m_param2 ? m_param2->text() : QString(), 1e6);
        cfg.pts = std::max(1, (m_param3 ? m_param3->text().trimmed().toInt() : 10));
        if (m_acSweepType) cfg.acSweepType = static_cast<SimAcSweepType>(m_acSweepType->currentIndex());
        cfg.rfPort1Source = m_param4 ? m_param4->text().trimmed() : "V1";
        cfg.rfPort2Node = m_param5 ? m_param5->text().trimmed() : "OUT";
        cfg.rfZ0 = parseValue(m_param6 ? m_param6->text().trimmed() : QString(), 50.0);
    } else if (idx == 6) {
        cfg.type = SimAnalysisType::MonteCarlo;
    } else if (idx == 7) {
        cfg.type = SimAnalysisType::ParametricSweep;
    } else if (idx == 8) {
        cfg.type = SimAnalysisType::Sensitivity;
    } else if (idx == 9) {
        cfg.type = SimAnalysisType::RealTime;
    }
    
    cfg.commandText = m_commandLine ? m_commandLine->text() : QString();
    return cfg;
}

bool SimulationPanel::isRealTimeMode() const {
    return m_analysisType && m_analysisType->currentIndex() == 9;
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

void SimulationPanel::onAcSweepTypeChanged() {
    if (!m_param1 || !m_param1->parentWidget()) return;
    QFormLayout* layout = qobject_cast<QFormLayout*>(m_param1->parentWidget()->layout());
    if (!layout) return;
    
    auto* ptsLabel = qobject_cast<QLabel*>(layout->labelForField(m_param3));
    if (!ptsLabel) return;

    int typeIdx = m_acSweepType->currentIndex();
    if (typeIdx == 0) ptsLabel->setText("Pts/Dec:");
    else if (typeIdx == 1) ptsLabel->setText("Pts/Oct:");
    else if (typeIdx == 2) ptsLabel->setText("Total Pts:");
}


