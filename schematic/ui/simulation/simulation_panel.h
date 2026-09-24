/*
 * Copyright 2026 Janada Sroor
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SIMULATION_PANEL_H
#define SIMULATION_PANEL_H

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QSplitter>
#include <QCheckBox>
#include <QMap>
#include <QPair>
#include <QVariantMap>
#include <QVector>
#include <QPointer>
#include <atomic>
#include <QDateTime>
#include <QElapsedTimer>
#include "../../../simulator/bridge/sim_manager.h"
#include "../../../simulator/core/sim_results.h"
#include "../../../simulator/core/sim_meas_evaluator.h"
#include "../../../ui/waveform_viewer.h"
#include "../smith_chart_widget.h"

class QGraphicsScene;
class SchematicEditor;
class NetManager;
class SimulationNetTableItem;
class SimulationDesignExplorerPanel;

class SimulationPanel : public QWidget {
    Q_OBJECT

public:
    explicit SimulationPanel(QGraphicsScene* scene, NetManager* netManager, const QString& projectDir = QString(), QWidget* parent = nullptr);
    ~SimulationPanel();

    void addProbe(const QString& signalName);
    void addDifferentialProbe(const QString& pNet, const QString& nNet);
    void removeProbe(const QString& signalName);
    void setEditor(SchematicEditor* editor) { m_editor = editor; }
    void clearAllProbes();
    void clearAllProbesPreserveX();
    void clearResults();
    bool hasProbe(const QString& signalName) const;
    void onRunSimulation();
    void updateSimulationDirectiveFromCurrentSettings();

    struct AnalysisConfig {
        SimAnalysisType type;
        double stop;
        double step;
        bool transientSteady = false;
        double steadyStateTol = 0.0;
        double steadyStateDelay = 0.0;
        double fStart;
        double fStop;
        int pts;
        SimAcSweepType acSweepType = SimAcSweepType::Decade;
        QString commandText;
        QString rfPort1Source;
        QString dcSource;
        QString rfPort2Node;
        double rfZ0;
    };
    void setAnalysisConfig(const AnalysisConfig& cfg);
    AnalysisConfig getAnalysisConfig() const;
    void setTargetScene(QGraphicsScene* scene, NetManager* netManager, const QString& projectDir, bool clearState = true);
    void removeTabState(QGraphicsScene* scene);
    QWidget* getOscilloscopeContainer() const;
    bool hasResults() const { return m_hasLastResults; }
    void showDetailedLog();
    bool isRealTimeMode() const;
    void setSchematicName(const QString& name);
    void plotResultsFromRaw(const QString& path);
    // Called when a netlist-editor run starts: stashes the current scene tab's
    // waveforms into per-tab state, then clears the views so stale data from
    // the previous tab can never be mistaken for the new run's results.
    void beginNetlistRun();

    struct TabOscilloscopeState {
        SimResults lastResults;
        SimResults previousResults;
        bool hasLastResults = false;
        bool hasPreviousResults = false;
        QList<WaveformViewer::SignalExport> waveformSignals;
        int waveformPaneCount = 0;
        int waveformFocusedPaneIndex = -1;
        QString selectedSignalName;
        struct SignalListItem {
            QString name;
            bool checked;
            QColor color;
        };
        QList<SignalListItem> signalListItems;
        struct ChartSeriesData {
            QString name;
            QVector<QPointF> points;
            QColor color;
            qreal penWidth = 1.0;
        };
        QList<ChartSeriesData> chartSeries;
        QList<ChartSeriesData> spectrumSeries;
        
        // Simulation parameters
        AnalysisConfig analysisConfig;
        QString commandText;
    };

Q_SIGNALS:
    void resultsReady(const SimResults& results);
    void realTimeBatchReady(const std::vector<double>& times, const std::vector<std::vector<double>>& values, const QStringList& names);
    void timeSnapshotReady(double t, const QMap<QString, double>& nodeVoltages, const QMap<QString, double>& branchCurrents);
    void probeRequested();
    void placementToolRequested(const QString& toolName);
    void analysisModeChanged();
    void simulationTargetRequested(const QString& targetType, const QString& targetId);
    void overlayVisibilityChanged(bool showVoltage, bool showCurrent);
    void clearOverlaysRequested();

public Q_SLOTS:
    void onClearFocusedPaneProbes();

public:
    void updateSchematicDirective();
    void updateSchematicDirectiveFromCommand(const QString& commandText);
    void cancelPendingRun();
    void setCurrentlyHoveredNet(const QString& netName);
    bool isSParameterModeSelected() const {
        return m_analysisType && m_analysisType->currentIndex() == 5;
    }

private Q_SLOTS:
    // --- UI Events & Simulation Callbacks ---
    void onAnalysisChanged(int index);
    void onAcSweepTypeChanged();
    void onLogReceived(const QString& msg);
    void onSimulationError(const QString& error);
    void onSimulationFinished();
    void onViewNetlist();
    void onSimResultsReady(const SimResults& results);
    void onRealTimeDataBatchReceived(const std::vector<double>& times, const std::vector<std::vector<double>>& values, const QStringList& names);
    void onTimelineValueChanged(int value);
    void onExportResultsCsv();
    void onExportResultsJson();
    void onExportResultsReport();
    void onMeasAdd();
    void onMeasRemove();
    void onMeasFunctionChanged(int index);
    void onSignalListContextMenuRequested(const QPoint& pos);

private:
    void setupUI();
    void createToolbar(QVBoxLayout* layout);
    void createSidebar(QSplitter* splitter);
    void createMainView(QSplitter* splitter);
    QWidget* createAnalysisSetupWidget();
    QWidget* createMonitorWidget();
    QWidget* createMeasurementsWidget();
    QString generateSpiceNetlist();
    void plotResults(const QString& rawData); 
    void plotBuiltinResults(const SimResults& results);
    void updateVirtualMeters(const SimResults& results);
    bool exportResultsCsvFile(const QString& path) const;
    bool exportResultsJsonFile(const QString& path) const;
    bool exportResultsReportFile(const QString& path) const;
    double parseValue(const QString& text, double defaultVal) const;
    double sampleAtX(const SimWaveform& wave, double x) const;
    double estimateFrequency(const SimWaveform& wave) const;
    double estimateFftPeakHz(const SimWaveform& wave) const;
    void updateChartRealTime(const QString& name, double t, double value);
    void appendIssueItem(const QString& msg);
    TabOscilloscopeState saveCurrentTabState() const;
    void restoreTabState(const TabOscilloscopeState& state);
    static QString tabStateKey(QGraphicsScene* scene);
    void parseCommandText(const QString& command, bool skipTypeOverride = false);
    void updateCommandDisplay();
    bool buildDerivedPowerWaveform(const QString& signalName, QVector<double>& time, QVector<double>& values) const;
    QStringList connectedNetsForItem(class SchematicItem* item, bool updateNets = true) const;
    void appendDerivedPowerWaveforms(SimResults& results) const;
    void appendEfficiencySummary(SimResults& results) const;
    void updateTransientNetTableOverlay(const SimResults& results);
    void clearTransientNetTableOverlay(QGraphicsScene* scene = nullptr);
    void refreshEfficiencyReport(const SimResults& results);
    bool shouldBuildStandardChart() const;
    bool shouldBuildSpectrumChart() const;
    void applyPlotQuality();
    WaveformViewer::PlotQuality selectedPlotQuality() const;
    bool shouldUseOpenGLRendering() const;
    bool shouldUseAntialiasing() const;
    int standardChartPointBudget() const;
    void syncSignalListFromWaveformViewer();
    QString formatFrequency(double freq) const;
    void syncFromSchematic();

    // --- Members & State ---
    QGraphicsScene* m_scene = nullptr;
    SchematicEditor* m_editor = nullptr;
    NetManager* m_netManager = nullptr;
    QString m_projectDir;
    std::atomic<bool> m_acceptRealTimeStream;

    // Signal Selection
    QListWidget* m_signalList = nullptr;
    QListWidget* m_issueList = nullptr;
    QTableWidget* m_measurementsTable = nullptr;
    
    // UI Elements
    QComboBox* m_analysisType = nullptr;
    QComboBox* m_acSweepType = nullptr;
    QLineEdit* m_commandLine = nullptr;
    QLineEdit* m_param1 = nullptr; // e.g., Start Time / Start Freq
    QLineEdit* m_param2 = nullptr; // e.g., Stop Time / Stop Freq
    QLineEdit* m_param3 = nullptr; // e.g., Step Size / Points
    QLineEdit* m_param4 = nullptr; 
    QLineEdit* m_param5 = nullptr; 
    QLineEdit* m_param6 = nullptr; 
    QCheckBox* m_steadyCheck = nullptr;
    QLineEdit* m_steadyTolEdit = nullptr;
    QLineEdit* m_steadyDelayEdit = nullptr;


    // Source generator controls
    class SimulationGeneratorPanel* m_generatorPanel = nullptr;
    
    QTextEdit* m_logOutput = nullptr;
    QPushButton* m_runButton = nullptr;
    QTimer* m_logFlushTimer = nullptr;
    QStringList m_logBuffer;
    
    // Timeline / Time-Travel
    QSlider* m_timelineSlider = nullptr;
    QLabel* m_timelineLabel = nullptr;
    QLabel* m_timelineIcon = nullptr;
    
    // Plotting
    QChartView* m_plotView = nullptr;
    QChart* m_chart = nullptr;
    
    QChartView* m_spectrumView = nullptr;
    QChart* m_spectrumChart = nullptr;
    QTabWidget* m_viewTabs = nullptr;
    QWidget* m_spectrumTab = nullptr;
    QWidget* m_designExplorerTab = nullptr;
    QComboBox* m_plotQualityCombo = nullptr;
    QLabel* m_schematicNameLabel = nullptr;
    SimulationDesignExplorerPanel* m_designExplorerPanel = nullptr;
    
    // Virtual Instruments
    class WaveformViewer* m_waveformViewer = nullptr;
    class QDoubleSpinBox* m_scopeTimeDiv = nullptr;
    class QDoubleSpinBox* m_scopeVoltDiv = nullptr;
    QComboBox* m_scopeChannelCombo = nullptr;
    QWidget* m_scopeContainer = nullptr;
    SmithChartWidget* m_smithChart = nullptr;
    QWidget* m_rfTab = nullptr;
    QWidget* m_efficiencyTab = nullptr;
    QLabel* m_efficiencySummaryLabel = nullptr;
    QTableWidget* m_efficiencyTable = nullptr;

    
    QString m_lastNetlistPath;
    QCheckBox* m_overlayPreviousRun;
    double m_cursorAFrac = 0.25;
    double m_cursorBFrac = 0.75;
    SimResults m_lastResults;
    SimResults m_previousResults;
    bool m_hasLastResults = false;
    bool m_buildInProgress = false;
    bool m_updatingDirective = false;
    quint64 m_runRequestSerial = 0;
    bool m_hasPreviousResults = false;
    QDateTime m_lastRunTimestampUtc;
    SimNetlist m_currentNetlist;
    QMap<QString, QLineSeries*> m_realTimeSeries;
    int m_realTimePointCounter = 0;
    QSet<QString> m_persistentCheckedSignals;
    bool m_isSimInitiator = false;
    QElapsedTimer m_liveSnapshotTimer;
    int m_liveSnapshotIntervalMs = 33; // ~30 FPS UI update ceiling
    
    // Performance optimization for real-time streaming with 'save all'
    QHash<QString, QString> m_liveNameCache;
    QSet<QString> m_schematicNets;
    QString m_currentlyHoveredNet;

    // Rolling cache of all signal data from live batches (for probe-during-pause)
    struct CachedSignal {
        QVector<double> time;
        QVector<double> values;
    };
    QMap<QString, CachedSignal> m_signalCache;
    int m_signalCacheMaxPoints = 100000;

    // .meas post-processing
    std::vector<MeasStatement> m_measStatements;
    void evaluateMeasStatements(const SimResults& results);

    // .meas editor UI
    QComboBox* m_measAnalysisType = nullptr;
    QComboBox* m_measFunction = nullptr;
    QLineEdit* m_measName = nullptr;
    QLineEdit* m_measSignal = nullptr;
    QLineEdit* m_measTrigSignal = nullptr;
    QLineEdit* m_measTrigVal = nullptr;
    QComboBox* m_measTrigEdge = nullptr;
    QLineEdit* m_measTargSignal = nullptr;
    QLineEdit* m_measTargVal = nullptr;
    QComboBox* m_measTargEdge = nullptr;
    QTableWidget* m_measListTable = nullptr;
    QPushButton* m_measAddBtn = nullptr;
    QPushButton* m_measRemoveBtn = nullptr;
    QWidget* m_measTrigTargWidget = nullptr;
    void rebuildMeasFromTable();
    QString generateMeasLine(int row) const;

    // Per-tab oscilloscope state persistence
    QMap<QGraphicsScene*, TabOscilloscopeState> m_tabStates;
    QMap<QGraphicsScene*, QPointer<SimulationNetTableItem>> m_netTableItems;
};

#endif // SIMULATION_PANEL_H
