#ifndef MOS_CIRCUIT_ARCHITECT_H
#define MOS_CIRCUIT_ARCHITECT_H

#include <QDialog>
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

class MosCircuitArchitect : public QDialog {
    Q_OBJECT

public:
    explicit MosCircuitArchitect(QWidget *parent = nullptr);
    ~MosCircuitArchitect() override;

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

    WaveformDrawWidget* m_drawWidget;
    QComboBox* m_topologyCombo;
    QTableWidget* m_paramTable;
    QCheckBox* m_stepCheck;
    QTextEdit* m_previewArea;
    QPushButton* m_generateBtn;
    QPushButton* m_verifyBtn;
    QString m_lastBitstream;
    
    QList<IMosWaveformSynthesizer*> m_synthesizers;
    IMosWaveformSynthesizer* m_currentSynthesizer;
};

#endif // MOS_CIRCUIT_ARCHITECT_H
