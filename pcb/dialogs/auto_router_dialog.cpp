#include "auto_router_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QGroupBox>
#include <QFormLayout>
#include <QMessageBox>

AutoRouterDialog::AutoRouterDialog(QGraphicsScene* scene, QWidget* parent)
    : QDialog(parent), m_scene(scene)
{
    setWindowTitle("Auto-Router");
    resize(550, 500);
    setupUI();
}

AutoRouterDialog::~AutoRouterDialog() = default;

void AutoRouterDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    m_tabs = new QTabWidget();
    setupOptionsTab();
    setupProgressTab();
    setupResultsTab();
    mainLayout->addWidget(m_tabs);
}

void AutoRouterDialog::setupOptionsTab() {
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);

    // Grid settings
    QGroupBox* gridGroup = new QGroupBox("Grid Settings");
    QFormLayout* gridLayout = new QFormLayout(gridGroup);
    m_gridSpacingSpin = new QDoubleSpinBox();
    m_gridSpacingSpin->setRange(0.1, 5.0);
    m_gridSpacingSpin->setSingleStep(0.1);
    m_gridSpacingSpin->setValue(0.5);
    m_gridSpacingSpin->setSuffix(" mm");
    gridLayout->addRow("Grid Spacing:", m_gridSpacingSpin);

    m_clearanceSpin = new QDoubleSpinBox();
    m_clearanceSpin->setRange(0.05, 2.0);
    m_clearanceSpin->setSingleStep(0.05);
    m_clearanceSpin->setValue(0.2);
    m_clearanceSpin->setSuffix(" mm");
    gridLayout->addRow("Clearance:", m_clearanceSpin);
    layout->addWidget(gridGroup);

    // Routing parameters
    QGroupBox* routeGroup = new QGroupBox("Routing Parameters");
    QFormLayout* routeLayout = new QFormLayout(routeGroup);

    m_maxIterSpin = new QSpinBox();
    m_maxIterSpin->setRange(1000, 500000);
    m_maxIterSpin->setSingleStep(5000);
    m_maxIterSpin->setValue(50000);
    routeLayout->addRow("Max A* Iterations:", m_maxIterSpin);

    m_maxRipUpRoundsSpin = new QSpinBox();
    m_maxRipUpRoundsSpin->setRange(0, 10);
    m_maxRipUpRoundsSpin->setValue(3);
    routeLayout->addRow("Rip-up Rounds:", m_maxRipUpRoundsSpin);

    layout->addWidget(routeGroup);

    // Layer selection
    QGroupBox* layerGroup = new QGroupBox("Layer Usage");
    QHBoxLayout* layerLayout = new QHBoxLayout(layerGroup);
    m_topLayerCheck = new QCheckBox("Top Layer");
    m_bottomLayerCheck = new QCheckBox("Bottom Layer");
    m_topLayerCheck->setChecked(true);
    m_bottomLayerCheck->setChecked(true);
    layerLayout->addWidget(m_topLayerCheck);
    layerLayout->addWidget(m_bottomLayerCheck);
    layerLayout->addStretch();
    layout->addWidget(layerGroup);

    // Advanced options
    QGroupBox* advGroup = new QGroupBox("Advanced");
    QVBoxLayout* advLayout = new QVBoxLayout(advGroup);
    m_diagonalCheck = new QCheckBox("Allow 45° diagonal moves (faster but less clean)");
    m_optimizeLengthCheck = new QCheckBox("Optimize for shortest trace length (Euclidean heuristic)");
    m_optimizeLengthCheck->setChecked(true);
    advLayout->addWidget(m_diagonalCheck);
    advLayout->addWidget(m_optimizeLengthCheck);
    layout->addWidget(advGroup);

    layout->addStretch();

    // Start button
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    m_startBtn = new QPushButton("🚀 Start Auto-Router");
    m_startBtn->setStyleSheet("background-color: #28a745; color: white; font-weight: bold; padding: 10px; font-size: 14px;");
    m_startBtn->setMinimumHeight(40);
    btnLayout->addWidget(m_startBtn);
    layout->addLayout(btnLayout);

    connect(m_startBtn, &QPushButton::clicked, this, &AutoRouterDialog::onStartRouting);
}

void AutoRouterDialog::setupProgressTab() {
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);

    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setMinimumHeight(30);
    layout->addWidget(m_progressBar);

    m_statusLabel = new QLabel("Ready to route");
    m_statusLabel->setStyleSheet("font-size: 14px; font-weight: bold; padding: 8px;");
    layout->addWidget(m_statusLabel);

    m_currentNetLabel = new QLabel();
    m_currentNetLabel->setStyleSheet("padding: 4px; color: palette(text);");
    layout->addWidget(m_currentNetLabel);

    m_logEdit = new QTextEdit();
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumHeight(200);
    layout->addWidget(m_logEdit);

    layout->addStretch();

    // Stop button
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    m_stopBtn = new QPushButton("⏹ Stop");
    m_stopBtn->setStyleSheet("background-color: #dc3545; color: white; font-weight: bold; padding: 8px;");
    btnLayout->addWidget(m_stopBtn);
    layout->addLayout(btnLayout);

    connect(m_stopBtn, &QPushButton::clicked, this, &AutoRouterDialog::onStopRouting);
}

void AutoRouterDialog::setupResultsTab() {
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);

    m_resultsEdit = new QTextEdit();
    m_resultsEdit->setReadOnly(true);
    layout->addWidget(m_resultsEdit);

    layout->addStretch();

    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    m_closeBtn = new QPushButton("Close");
    btnLayout->addWidget(m_closeBtn);
    layout->addLayout(btnLayout);

    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
}

PCBAutoRouter::RouterConfig AutoRouterDialog::collectConfig() {
    PCBAutoRouter::RouterConfig config;
    config.gridSpacing = m_gridSpacingSpin->value();
    config.clearance = m_clearanceSpin->value();
    config.viaClearance = m_clearanceSpin->value() * 1.5;
    config.maxIterations = m_maxIterSpin->value();
    config.maxRipUpRounds = m_maxRipUpRoundsSpin->value();
    config.preferTopLayer = m_topLayerCheck->isChecked();
    config.preferBottomLayer = m_bottomLayerCheck->isChecked();
    config.allowDiagonals = m_diagonalCheck->isChecked();
    config.optimizeTraceLength = m_optimizeLengthCheck->isChecked();
    config.reportProgress = true;
    config.progressInterval = 5;
    return config;
}

void AutoRouterDialog::onStartRouting() {
    if (!m_scene) {
        QMessageBox::warning(this, "No PCB Scene", "No PCB scene available for routing.");
        return;
    }

    m_startBtn->setEnabled(false);
    m_tabs->setCurrentIndex(1); // Switch to progress tab
    m_progressBar->setValue(0);
    m_statusLabel->setText("Initializing...");
    m_logEdit->clear();
    m_routingComplete = false;

    // Create and run router in a background thread
    m_router = new PCBAutoRouter(m_scene, this);

    // Connect signals
    connect(m_router, &PCBAutoRouter::connectionRouted,
            this, &AutoRouterDialog::onConnectionRouted);
    connect(m_router, &PCBAutoRouter::connectionFailed,
            this, &AutoRouterDialog::onConnectionFailed);
    connect(m_router, &PCBAutoRouter::progressChanged,
            this, &AutoRouterDialog::onProgressChanged);
    connect(m_router, &PCBAutoRouter::routingFinished,
            this, &AutoRouterDialog::onRoutingFinished);

    // Run router (blocking call, but emits signals during progress)
    auto config = collectConfig();
    m_logEdit->append(QString("Grid spacing: %1 mm").arg(config.gridSpacing));
    m_logEdit->append(QString("Clearance: %1 mm").arg(config.clearance));
    m_logEdit->append("Starting auto-router...");
    m_logEdit->append("");

    auto stats = m_router->routeAll(config);
    displayResults(stats);
}

void AutoRouterDialog::onStopRouting() {
    if (m_router && m_router->isRunning()) {
        m_router->requestStop();
        m_statusLabel->setText("Stopping...");
        m_stopBtn->setEnabled(false);
    }
}

void AutoRouterDialog::onConnectionRouted(const QString& netName, int current, int total) {
    m_currentNetLabel->setText(QString("Routing: %1 (%2/%3)").arg(netName).arg(current).arg(total));
    m_logEdit->append(QString("✅ Routed: %1 (%2/%3)").arg(netName).arg(current).arg(total));
    m_progressBar->setValue(static_cast<int>(100.0 * current / total));
}

void AutoRouterDialog::onConnectionFailed(const QString& netName, int current, int total) {
    m_currentNetLabel->setText(QString("Failed: %1 (%2/%3)").arg(netName).arg(current).arg(total));
    m_logEdit->append(QString("❌ Failed: %1 (%2/%3)").arg(netName).arg(current).arg(total));
    m_progressBar->setValue(static_cast<int>(100.0 * current / total));
}

void AutoRouterDialog::onProgressChanged(double progress, const QString& status) {
    m_statusLabel->setText(status);
    m_progressBar->setValue(static_cast<int>(100.0 * progress));
}

void AutoRouterDialog::onRoutingFinished(const PCBAutoRouter::RouteStats& stats) {
    m_routingComplete = true;
    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(true);
    displayResults(stats);
    m_tabs->setCurrentIndex(2); // Switch to results tab
}

void AutoRouterDialog::displayResults(const PCBAutoRouter::RouteStats& stats) {
    QString html;
    html += "<h2>Auto-Router Results</h2>";
    html += "<table style='width: 100%; font-size: 14px;'>";

    double completionRate = stats.totalConnections > 0
        ? 100.0 * stats.routedConnections / stats.totalConnections : 0.0;

    QString color = completionRate >= 90 ? "#28a745" : completionRate >= 50 ? "#ffc107" : "#dc3545";

    html += QString("<tr><td><b>Status:</b></td><td style='color: %1; font-weight: bold;'>%2</td></tr>")
        .arg(color)
        .arg(completionRate >= 100 ? "✅ All connections routed!" :
             completionRate >= 50 ? "⚠️ Partial routing" : "❌ Poor routing completion");
    html += QString("<tr><td><b>Completion:</b></td><td><b>%1%</b> (%2/%3)</td></tr>")
        .arg(completionRate, 0, 'f', 1).arg(stats.routedConnections).arg(stats.totalConnections);
    html += QString("<tr><td><b>Failed:</b></td><td>%1</td></tr>").arg(stats.failedConnections);
    html += "<tr><td colspan='2'><hr></td></tr>";
    html += QString("<tr><td><b>Trace Segments:</b></td><td>%1</td></tr>").arg(stats.traceSegmentCount);
    html += QString("<tr><td><b>Vias Placed:</b></td><td>%1</td></tr>").arg(stats.viaCount);
    html += QString("<tr><td><b>Total Trace Length:</b></td><td>%1 mm</td></tr>")
        .arg(stats.totalTraceLength, 0, 'f', 2);
    html += QString("<tr><td><b>Rip-up Count:</b></td><td>%1</td></tr>").arg(stats.ripUpCount);
    html += QString("<tr><td><b>A* Iterations:</b></td><td>%1</td></tr>").arg(stats.iterations);

    html += "</table>";

    if (!stats.failedNets.isEmpty()) {
        html += "<h3>Failed Nets</h3><ul>";
        QSet<QString> uniqueFailed(stats.failedNets.begin(), stats.failedNets.end());
        for (const QString& net : uniqueFailed.values()) {
            html += QString("<li style='color: #dc3545;'>%1</li>").arg(net);
        }
        html += "</ul>";
    }

    m_resultsEdit->setHtml(html);
    m_logEdit->append("");
    m_logEdit->append(QString("=== Routing Complete: %1/%2 routed (%3%) ===")
        .arg(stats.routedConnections).arg(stats.totalConnections)
        .arg(completionRate, 0, 'f', 1));
}
