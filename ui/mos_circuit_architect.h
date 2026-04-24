#ifndef MOS_CIRCUIT_ARCHITECT_H
#define MOS_CIRCUIT_ARCHITECT_H

#include <QMainWindow>
#include <QTableWidget>
#include <QComboBox>
#include <QTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QVector>
#include <QPointF>
#include <QMap>

#include "../simulator/synthesis/mos_synthesizer_interface.h"

class WaveformDrawWidget; 
class QCheckBox;
class WaveformViewer;
class VoltageSourceItem;
struct SimResults;

class MosCircuitArchitect : public QMainWindow {
    Q_OBJECT

public:
    explicit MosCircuitArchitect(QWidget *parent = nullptr);
    ~MosCircuitArchitect() override;

    void setSourceItem(VoltageSourceItem* item);
    void setPoints(const QVector<QPointF>& points);

private Q_SLOTS:
    void onTopologyChanged(int index);
    void onGenerateClicked();
    void onVerifyClicked();
    void onParameterChanged();
    void onSquareClicked();
    void onBitstreamClicked();

private:
    void setupUi();
    void loadTopologies();
    void populateParameters();
    void updatePreview();
    void updateSimulationVision();

    WaveformDrawWidget* m_drawWidget;
    QComboBox* m_topologyCombo;
    QTableWidget* m_paramTable;
    QCheckBox* m_stepCheck;
    QTextEdit* m_previewArea;
    QPushButton* m_runBtn;
    QString m_lastBitstream;
    VoltageSourceItem* m_sourceItem;

    QList<IMosWaveformSynthesizer*> m_synthesizers;
    IMosWaveformSynthesizer* m_currentSynthesizer;
};

#endif // MOS_CIRCUIT_ARCHITECT_H
