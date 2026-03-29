#include "oscilloscope_window.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QCloseEvent>
#include <QDebug>
#include "flux/core/net_manager.h"

OscilloscopeWindow::OscilloscopeWindow(const QUuid& itemId, const QString& itemName, QWidget* parent)
    : QMainWindow(parent), m_itemId(itemId), m_itemName(itemName) {
    
    setWindowTitle(QString("Oscilloscope: %1").arg(itemName));
    setMinimumSize(800, 500);
    
    // Default initial config
    m_config.timebase = 1e-3;
    m_config.triggerSource = "CH1";
    m_config.triggerLevel = 0.0;
    for (int i = 0; i < 4; ++i) {
        m_config.channels[i].enabled = true;
        m_config.channels[i].scale = 1.0;
        m_config.channels[i].offset = 0.0;
    }
    m_config.channels[0].color = Qt::yellow;
    m_config.channels[1].color = Qt::cyan;
    m_config.channels[2].color = Qt::magenta;
    m_config.channels[3].color = QColor(0, 255, 100);

    setupUI();
}

OscilloscopeWindow::~OscilloscopeWindow() {}

void OscilloscopeWindow::setupUI() {
    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    
    QHBoxLayout* mainLayout = new QHBoxLayout(central);
    
    // 1. Plot Area
    m_scopeDisplay = new MiniScopeWidget(this);
    mainLayout->addWidget(m_scopeDisplay, 3);
    
    // 2. Control Panel
    QVBoxLayout* controlLayout = new QVBoxLayout();
    mainLayout->addLayout(controlLayout, 1);
    
    // Channel Groups
    for (int i = 0; i < 4; ++i) {
        QGroupBox* chGroup = new QGroupBox(QString("CH%1").arg(i + 1), this);
        QGridLayout* gl = new QGridLayout(chGroup);
        
        m_channelUI[i].enabled = new QCheckBox("Enabled", this);
        m_channelUI[i].enabled->setChecked(m_config.channels[i].enabled);
        gl->addWidget(m_channelUI[i].enabled, 0, 0, 1, 2);
        
        gl->addWidget(new QLabel("V/Div:", this), 1, 0);
        m_channelUI[i].voltsDiv = new QDoubleSpinBox(this);
        m_channelUI[i].voltsDiv->setRange(0.001, 1000.0);
        m_channelUI[i].voltsDiv->setValue(1.0 / m_config.channels[i].scale);
        gl->addWidget(m_channelUI[i].voltsDiv, 1, 1);
        
        gl->addWidget(new QLabel("Offset:", this), 2, 0);
        m_channelUI[i].offset = new QDoubleSpinBox(this);
        m_channelUI[i].offset->setRange(-1000.0, 1000.0);
        m_channelUI[i].offset->setValue(m_config.channels[i].offset);
        gl->addWidget(m_channelUI[i].offset, 2, 1);
        
        controlLayout->addWidget(chGroup);
        
        connect(m_channelUI[i].enabled, &QCheckBox::toggled, [this, i](bool c) { onChannelToggled(i, c); });
        connect(m_channelUI[i].voltsDiv, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this, i](double v) { onVoltsDivChanged(i, v); });
        connect(m_channelUI[i].offset, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this, i](double v) { onOffsetChanged(i, v); });
    }
    
    // Horizontal / Trigger Group
    QGroupBox* hGroup = new QGroupBox("Horizontal / Trigger", this);
    QGridLayout* hGl = new QGridLayout(hGroup);
    
    hGl->addWidget(new QLabel("T/Div:", this), 0, 0);
    m_timebaseSpin = new QDoubleSpinBox(this);
    m_timebaseSpin->setRange(1e-9, 10.0);
    m_timebaseSpin->setDecimals(9);
    m_timebaseSpin->setValue(m_config.timebase);
    hGl->addWidget(m_timebaseSpin, 0, 1);
    
    hGl->addWidget(new QLabel("Trig Src:", this), 1, 0);
    m_triggerSourceCombo = new QComboBox(this);
    m_triggerSourceCombo->addItems({"CH1", "CH2", "CH3", "CH4"});
    hGl->addWidget(m_triggerSourceCombo, 1, 1);
    
    hGl->addWidget(new QLabel("Trig Lvl:", this), 2, 0);
    m_triggerLevelSpin = new QDoubleSpinBox(this);
    m_triggerLevelSpin->setRange(-1000.0, 1000.0);
    m_triggerLevelSpin->setValue(m_config.triggerLevel);
    hGl->addWidget(m_triggerLevelSpin, 2, 1);
    
    controlLayout->addWidget(hGroup);
    
    QPushButton* propBtn = new QPushButton("Properties...", this);
    propBtn->setStyleSheet("background-color: #3b3b3b; color: #fff; border: 1px solid #555; padding: 5px;");
    controlLayout->addWidget(propBtn);
    
    controlLayout->addStretch();
    
    connect(propBtn, &QPushButton::clicked, [this]() { emit propertiesRequested(m_itemId); });
    
    connect(m_timebaseSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &OscilloscopeWindow::onTimebaseChanged);
    connect(m_triggerSourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OscilloscopeWindow::onTriggerSourceChanged);
    connect(m_triggerLevelSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &OscilloscopeWindow::onTriggerLevelChanged);
}

void OscilloscopeWindow::updateResults(const SimResults& results, NetManager* netManager) {
    if (!netManager) return;
    m_lastNetManager = netManager;

    QMap<QString, QVector<QPointF>> visibleTraces;
    
    for (int i = 0; i < 4; ++i) {
        if (!m_config.channels[i].enabled) continue;
        
        QString traceName = QString("V(%1_%2)").arg(m_itemName).arg(i); 
        
        const SimWaveform* targetWave = nullptr;
        for (const auto& wave : results.waveforms) {
            if (QString::fromStdString(wave.name) == traceName) {
                targetWave = &wave;
                break;
            }
        }
        
        if (targetWave) {
            QVector<QPointF> points;
            points.reserve(targetWave->xData.size());
            
            double scale = m_config.channels[i].scale;
            double offset = m_config.channels[i].offset;
            
            for (size_t s = 0; s < targetWave->xData.size(); ++s) {
                points.append(QPointF(targetWave->xData[s], (targetWave->yData[s] * scale) + offset));
            }
            visibleTraces[QString("CH%1").arg(i+1)] = points;
        }
    }
    
    m_scopeDisplay->setMultiTraceData(visibleTraces);
}

void OscilloscopeWindow::clear() {
    m_scopeDisplay->clear();
}

void OscilloscopeWindow::closeEvent(QCloseEvent* event) {
    emit windowClosing(m_itemId);
    event->accept();
}

void OscilloscopeWindow::onChannelToggled(int ch, bool checked) {
    m_config.channels[ch].enabled = checked;
    emit configChanged(m_itemId, m_config);
}

void OscilloscopeWindow::onTimebaseChanged(double value) {
    m_config.timebase = value;
    emit configChanged(m_itemId, m_config);
}

void OscilloscopeWindow::onVoltsDivChanged(int ch, double value) {
    if (value > 0) m_config.channels[ch].scale = 1.0 / value;
    emit configChanged(m_itemId, m_config);
}

void OscilloscopeWindow::onOffsetChanged(int ch, double value) {
    m_config.channels[ch].offset = value;
    emit configChanged(m_itemId, m_config);
}

void OscilloscopeWindow::onTriggerSourceChanged(int index) {
    m_config.triggerSource = QString("CH%1").arg(index + 1);
    emit configChanged(m_itemId, m_config);
}

void OscilloscopeWindow::onTriggerLevelChanged(double value) {
    m_config.triggerLevel = value;
    emit configChanged(m_itemId, m_config);
}
