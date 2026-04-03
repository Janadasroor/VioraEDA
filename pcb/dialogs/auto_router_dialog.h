#ifndef AUTO_ROUTER_DIALOG_H
#define AUTO_ROUTER_DIALOG_H

#include <QDialog>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QTextEdit>
#include <QGroupBox>
#include <QTabWidget>
#include "pcb_auto_router.h"

class QGraphicsScene;

/**
 * @brief Dialog for configuring and running the PCB auto-router.
 */
class AutoRouterDialog : public QDialog {
    Q_OBJECT

public:
    explicit AutoRouterDialog(QGraphicsScene* scene, QWidget* parent = nullptr);
    ~AutoRouterDialog();

private slots:
    void onStartRouting();
    void onStopRouting();
    void onConnectionRouted(const QString& netName, int current, int total);
    void onConnectionFailed(const QString& netName, int current, int total);
    void onProgressChanged(double progress, const QString& status);
    void onRoutingFinished(const PCBAutoRouter::RouteStats& stats);

private:
    void setupUI();
    void setupOptionsTab();
    void setupProgressTab();
    void setupResultsTab();
    PCBAutoRouter::RouterConfig collectConfig();
    void displayResults(const PCBAutoRouter::RouteStats& stats);

    QGraphicsScene* m_scene;
    QTabWidget* m_tabs;

    // Options tab
    QDoubleSpinBox* m_gridSpacingSpin;
    QDoubleSpinBox* m_clearanceSpin;
    QSpinBox* m_maxIterSpin;
    QSpinBox* m_maxRipUpRoundsSpin;
    QCheckBox* m_topLayerCheck;
    QCheckBox* m_bottomLayerCheck;
    QCheckBox* m_diagonalCheck;
    QCheckBox* m_optimizeLengthCheck;
    QPushButton* m_startBtn;

    // Progress tab
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    QLabel* m_currentNetLabel;
    QTextEdit* m_logEdit;
    QPushButton* m_stopBtn;

    // Results tab
    QTextEdit* m_resultsEdit;
    QPushButton* m_closeBtn;

    PCBAutoRouter* m_router = nullptr;
    bool m_routingComplete = false;
};

#endif // AUTO_ROUTER_DIALOG_H
