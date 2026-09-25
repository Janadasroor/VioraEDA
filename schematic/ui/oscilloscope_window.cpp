/*
 * Copyright 2026 Janada Sroor
 * SPDX-License-Identifier: Apache-2.0
 */

#include "oscilloscope_window.h"
#if __has_include("../../core/remote_display_server.h") && __has_include(<QtWebSockets/QWebSocketServer>)
#include "remote_display_server.h"
#define VIOSPICE_HAS_REMOTE_DISPLAY 1
#endif
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QScrollArea>
#include <QCloseEvent>
#include <QDebug>
#include <QJsonArray>
#include <QDateTime>
#include <algorithm>
#include "net_manager.h"

OscilloscopeWindow::OscilloscopeWindow(const QUuid& itemId, const QString& itemName, QWidget* parent)
    : QMainWindow(parent), m_itemId(itemId), m_itemName(itemName) {
    
    setWindowTitle(QString("Oscilloscope: %1").arg(itemName));
    setObjectName("OscilloscopeWindow");
    setMinimumSize(850, 520);
    
    // Default initial config
    m_config = OscilloscopeItem::Config();

    setupUI();
}

OscilloscopeWindow::~OscilloscopeWindow() {}

void OscilloscopeWindow::setConfig(const OscilloscopeItem::Config& cfg) {
    m_config = cfg;
    
    // Update trigger source combo choices
    m_triggerSourceCombo->blockSignals(true);
    m_triggerSourceCombo->clear();
    for (int i = 1; i <= m_config.channelCount; ++i) {
        m_triggerSourceCombo->addItem(QString("CH%1").arg(i));
    }
    m_triggerSourceCombo->setCurrentText(m_config.triggerSource);
    m_triggerSourceCombo->blockSignals(false);

    m_timebaseSpin->blockSignals(true);
    m_timebaseSpin->setValue(m_config.timebase);
    m_timebaseSpin->blockSignals(false);

    m_triggerLevelSpin->blockSignals(true);
    m_triggerLevelSpin->setValue(m_config.triggerLevel);
    m_triggerLevelSpin->blockSignals(false);

    rebuildChannelControls();
}

void OscilloscopeWindow::setupUI() {
    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    
    QHBoxLayout* mainLayout = new QHBoxLayout(central);
    
    // 1. Plot Area
    m_scopeDisplay = new MiniScopeWidget(this);
    mainLayout->addWidget(m_scopeDisplay, 3);
    
    // 2. Control Panel
    QWidget* controlPanel = new QWidget(this);
    controlPanel->setFixedWidth(240);
    QVBoxLayout* controlLayout = new QVBoxLayout(controlPanel);
    controlLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->addWidget(controlPanel);

    // Scroll Area for dynamic channels
    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    QWidget* channelsContainer = new QWidget(scrollArea);
    m_channelsContainerLayout = new QVBoxLayout(channelsContainer);
    m_channelsContainerLayout->setContentsMargins(0, 0, 0, 0);
    m_channelsContainerLayout->setSpacing(4);
    scrollArea->setWidget(channelsContainer);
    controlLayout->addWidget(scrollArea, 1);

    rebuildChannelControls();
    
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
    for (int i = 1; i <= m_config.channelCount; ++i) {
        m_triggerSourceCombo->addItem(QString("CH%1").arg(i));
    }
    m_triggerSourceCombo->setCurrentText(m_config.triggerSource);
    hGl->addWidget(m_triggerSourceCombo, 1, 1);
    
    hGl->addWidget(new QLabel("Trig Lvl:", this), 2, 0);
    m_triggerLevelSpin = new QDoubleSpinBox(this);
    m_triggerLevelSpin->setRange(-1000.0, 1000.0);
    m_triggerLevelSpin->setValue(m_config.triggerLevel);
    hGl->addWidget(m_triggerLevelSpin, 2, 1);
    
    controlLayout->addWidget(hGroup);

    // Cursors & Measurements Group
    QGroupBox* curGroup = new QGroupBox("Precision Cursors", this);
    QVBoxLayout* curVl = new QVBoxLayout(curGroup);
    curVl->setSpacing(4);

    QHBoxLayout* curModeHl = new QHBoxLayout();
    curModeHl->addWidget(new QLabel("Mode:", this));
    m_cursorModeCombo = new QComboBox(this);
    m_cursorModeCombo->addItems({"Off", "Time (X1, X2)", "Voltage (Y1, Y2)", "Both (X & Y)"});
    curModeHl->addWidget(m_cursorModeCombo, 1);
    curVl->addLayout(curModeHl);

    m_cursorDeltaTimeLabel = new QLabel("ΔX: --", this);
    m_cursorDeltaTimeLabel->setStyleSheet("color: #00f0ff; font-family: monospace; font-size: 10px; font-weight: bold;");
    m_cursorFreqLabel = new QLabel("1/ΔX: --", this);
    m_cursorFreqLabel->setStyleSheet("color: #00f0ff; font-family: monospace; font-size: 10px; font-weight: bold;");
    m_cursorDeltaVoltLabel = new QLabel("ΔY: --", this);
    m_cursorDeltaVoltLabel->setStyleSheet("color: #ff78dc; font-family: monospace; font-size: 10px; font-weight: bold;");

    curVl->addWidget(m_cursorDeltaTimeLabel);
    curVl->addWidget(m_cursorFreqLabel);
    curVl->addWidget(m_cursorDeltaVoltLabel);
    controlLayout->addWidget(curGroup);

    // Waveform Memory Group
    QGroupBox* memGroup = new QGroupBox("Waveform Controls", this);
    QVBoxLayout* memVl = new QVBoxLayout(memGroup);
    m_freezeBtn = new QPushButton("Freeze Traces", this);
    m_freezeBtn->setStyleSheet(
        "QPushButton { background-color: #2563eb; color: white; font-weight: bold; padding: 6px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #1d4ed8; }"
    );
    m_clearMemBtn = new QPushButton("Clear Memories", this);
    m_clearMemBtn->setStyleSheet(
        "QPushButton { background-color: #3f3f46; color: #d1d5db; padding: 4px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #52525b; }"
    );
    memVl->addWidget(m_freezeBtn);
    memVl->addWidget(m_clearMemBtn);
    controlLayout->addWidget(memGroup);
    
    QPushButton* propBtn = new QPushButton("Instrument Properties...", this);
    propBtn->setStyleSheet("background-color: #3b3b3b; color: #fff; border: 1px solid #555; padding: 6px; border-radius: 4px;");
    controlLayout->addWidget(propBtn);
    
    connect(m_scopeDisplay, &MiniScopeWidget::zoomToFitRequested, this, &OscilloscopeWindow::zoomToFit);
    connect(m_scopeDisplay, &MiniScopeWidget::fitYAxisRequested, this, &OscilloscopeWindow::fitYAxis);
    connect(m_scopeDisplay, &MiniScopeWidget::timebaseChanged, this, [this](double tb) {
        m_config.timebase = tb;
        if (m_timebaseSpin) {
            m_timebaseSpin->blockSignals(true);
            m_timebaseSpin->setValue(tb);
            m_timebaseSpin->blockSignals(false);
        }
        Q_EMIT configChanged(m_itemId, m_config);
    });
    connect(m_scopeDisplay, &MiniScopeWidget::rubberBandZoomCompleted, this, [this](double /*tMin*/, double /*tMax*/, double yMinDiv, double yMaxDiv) {
        double divSpan = std::abs(yMaxDiv - yMinDiv);
        if (divSpan > 0.3) {
            double centerDiv = (yMinDiv + yMaxDiv) / 2.0;
            double zoomFactor = 8.0 / divSpan;
            int count = m_config.channelCount;
            for (int i = 0; i < count && i < m_config.channels.size(); ++i) {
                if (!m_config.channels[i].enabled) continue;
                double oldScale = m_config.channels[i].scale;
                double oldOffset = m_config.channels[i].offset;
                double newScale = oldScale * zoomFactor;
                double newOffset = (oldOffset - centerDiv) * zoomFactor;
                m_config.channels[i].scale = newScale;
                m_config.channels[i].offset = newOffset;
                if (i < m_channelUIs.size()) {
                    if (m_channelUIs[i].voltsDiv) {
                        m_channelUIs[i].voltsDiv->blockSignals(true);
                        m_channelUIs[i].voltsDiv->setValue(1.0 / std::max(1e-6, newScale));
                        m_channelUIs[i].voltsDiv->blockSignals(false);
                    }
                    if (m_channelUIs[i].offset) {
                        m_channelUIs[i].offset->blockSignals(true);
                        m_channelUIs[i].offset->setValue(newOffset);
                        m_channelUIs[i].offset->blockSignals(false);
                    }
                }
            }
            Q_EMIT configChanged(m_itemId, m_config);
            reprocessTraces();
        }
    });
    
    connect(m_cursorModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        m_scopeDisplay->setCursorMode(static_cast<MiniScopeWidget::CursorMode>(idx));
        bool hasTime = (idx == 1 || idx == 3);
        bool hasVolt = (idx == 2 || idx == 3);
        m_cursorDeltaTimeLabel->setVisible(hasTime);
        m_cursorFreqLabel->setVisible(hasTime);
        m_cursorDeltaVoltLabel->setVisible(hasVolt);
    });

    connect(m_scopeDisplay, &MiniScopeWidget::cursorsChanged, this, [this](double dt, double f, double dv) {
        auto formatValueSI = [](double val, const QString& unit) {
            const double absVal = std::abs(val);
            if (absVal < 1e-18) return "0" + unit;
            static const struct { double mult; const char* sym; } suffixes[] = {
                {1e12, "T"}, {1e9, "G"}, {1e6, "Meg"}, {1e3, "k"},
                {1.0, ""},
                {1e-3, "m"}, {1e-6, "u"}, {1e-9, "n"}, {1e-12, "p"}, {1e-15, "f"}
            };
            for (const auto& s : suffixes) {
                if (absVal >= s.mult * 0.999) {
                    QString num = QString::number(val / s.mult, 'f', 2);
                    return num + s.sym + unit;
                }
            }
            return QString::number(val, 'g', 4) + unit;
        };

        m_cursorDeltaTimeLabel->setText("ΔX:   " + formatValueSI(dt, "s"));
        m_cursorFreqLabel->setText("1/ΔX: " + formatValueSI(f, "Hz"));
        m_cursorDeltaVoltLabel->setText("ΔY:   " + formatValueSI(dv, "V"));
    });

    m_cursorDeltaTimeLabel->hide();
    m_cursorFreqLabel->hide();
    m_cursorDeltaVoltLabel->hide();

    connect(m_timebaseSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &OscilloscopeWindow::onTimebaseChanged);
    connect(m_triggerSourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OscilloscopeWindow::onTriggerSourceChanged);
    connect(m_triggerLevelSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &OscilloscopeWindow::onTriggerLevelChanged);
    connect(m_freezeBtn, &QPushButton::clicked, this, &OscilloscopeWindow::onFreezeClicked);
    connect(m_clearMemBtn, &QPushButton::clicked, this, &OscilloscopeWindow::onClearMemoriesClicked);
}

void OscilloscopeWindow::rebuildChannelControls() {
    if (!m_channelsContainerLayout) return;

    // Clear old UI
    QLayoutItem* item;
    while ((item = m_channelsContainerLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    m_channelUIs.clear();

    int count = m_config.channelCount;
    m_channelUIs.resize(count);

    for (int i = 0; i < count; ++i) {
        const auto& ch = (i < m_config.channels.size()) ? m_config.channels[i] : OscilloscopeItem::ChannelConfig();
        
        QGroupBox* chGroup = new QGroupBox(QString("CH%1 (+/- Probes)").arg(i + 1), this);
        chGroup->setStyleSheet(QString("QGroupBox::title { color: %1; font-weight: bold; }").arg(ch.color.name()));
        QGridLayout* gl = new QGridLayout(chGroup);
        gl->setContentsMargins(4, 4, 4, 4);

        m_channelUIs[i].group = chGroup;

        m_channelUIs[i].enabled = new QCheckBox("Active", this);
        m_channelUIs[i].enabled->setChecked(ch.enabled);
        gl->addWidget(m_channelUIs[i].enabled, 0, 0);

        m_channelUIs[i].floating = new QCheckBox("Floating Ref", this);
        m_channelUIs[i].floating->setToolTip("Measure (CH+ - CH-) across floating nodes");
        m_channelUIs[i].floating->setChecked(ch.floatingGround);
        gl->addWidget(m_channelUIs[i].floating, 0, 1);

        gl->addWidget(new QLabel("V/Div:", this), 1, 0);
        m_channelUIs[i].voltsDiv = new QDoubleSpinBox(this);
        m_channelUIs[i].voltsDiv->setRange(0.001, 1000.0);
        m_channelUIs[i].voltsDiv->setValue(ch.scale > 0 ? (1.0 / ch.scale) : 1.0);
        gl->addWidget(m_channelUIs[i].voltsDiv, 1, 1);

        gl->addWidget(new QLabel("Offset:", this), 2, 0);
        m_channelUIs[i].offset = new QDoubleSpinBox(this);
        m_channelUIs[i].offset->setRange(-1000.0, 1000.0);
        m_channelUIs[i].offset->setValue(ch.offset);
        gl->addWidget(m_channelUIs[i].offset, 2, 1);

        m_channelsContainerLayout->addWidget(chGroup);

        connect(m_channelUIs[i].enabled, &QCheckBox::toggled, [this, i](bool c) { onChannelToggled(i, c); });
        connect(m_channelUIs[i].floating, &QCheckBox::toggled, [this, i](bool c) { onFloatingToggled(i, c); });
        connect(m_channelUIs[i].voltsDiv, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this, i](double v) { onVoltsDivChanged(i, v); });
        connect(m_channelUIs[i].offset, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this, i](double v) { onOffsetChanged(i, v); });
    }
}

void OscilloscopeWindow::updateRealTimeData(const std::vector<double>& times, const std::vector<std::vector<double>>& values, const QStringList& names, SchematicItem* item, NetManager* netManager) {
    if (times.empty() || values.empty() || names.isEmpty()) return;

    if (netManager) {
        m_lastNetManager = netManager;
    }
    if (item) {
        m_cachedItem = item;
    }

    QMap<QString, QVector<QPointF>> visibleTraces;
    int count = m_config.channelCount;
    
    auto cleanNetName = [](const QString& s) -> QString {
        QString t = s.trimmed();
        if (t.startsWith("v(", Qt::CaseInsensitive) && t.endsWith(")")) {
            t = t.mid(2, t.length() - 3).trimmed();
        }
        return t;
    };

    auto findWaveIdx = [&](const QString& net) -> int {
        if (net.isEmpty()) return -1;
        const QString cNet = cleanNetName(net);
        for (int idx = 0; idx < names.size(); ++idx) {
            const QString cName = cleanNetName(names[idx]);
            if (cName.compare(cNet, Qt::CaseInsensitive) == 0) {
                return idx;
            }
        }
        return -1;
    };

    for (int i = 0; i < count && i < m_config.channels.size(); ++i) {
        if (!m_config.channels[i].enabled) continue;
        
        QString posNet, negNet;
        if (m_cachedItem && m_lastNetManager) {
            const auto pts = m_cachedItem->connectionPoints();
            if (i < pts.size()) {
                posNet = m_lastNetManager->findNetAtPoint(m_cachedItem->mapToScene(pts[i]));
            }
            if (count + i < pts.size()) {
                negNet = m_lastNetManager->findNetAtPoint(m_cachedItem->mapToScene(pts[count + i]));
            }
        }

        // If the pin is not connected to a net, do not pick arbitrary waveforms
        if (posNet.isEmpty()) {
            continue;
        }

        int posIdx = findWaveIdx(posNet);
        if (posIdx < 0) continue;

        int negIdx = findWaveIdx(negNet);

        QVector<QPointF> points;
        points.reserve(times.size());

        double scale = m_config.channels[i].scale;
        double offset = m_config.channels[i].offset;
        bool floating = m_config.channels[i].floatingGround;

        for (size_t s = 0; s < times.size(); ++s) {
            if (s >= values.size()) break;
            const auto& row = values[s];
            if (posIdx >= (int)row.size()) break;

            double v = row[posIdx];
            if (floating && negIdx >= 0 && negIdx < (int)row.size()) {
                v -= row[negIdx];
            }
            points.append(QPointF(times[s], (v * scale) + offset));
        }

        if (!points.isEmpty()) {
            visibleTraces[QString("CH%1").arg(i+1)] = points;
        }
    }

    QMap<QString, QColor> traceColors;
    for (int i = 0; i < count && i < m_config.channels.size(); ++i) {
        traceColors[QString("CH%1").arg(i + 1)] = m_config.channels[i].color;
    }

    if (m_scopeDisplay) {
        m_scopeDisplay->setTimebase(m_config.timebase);
        if (!visibleTraces.isEmpty()) {
            m_scopeDisplay->appendMultiTraceData(visibleTraces, traceColors);
        }
    }
}

void OscilloscopeWindow::updateResults(const SimResults& results, NetManager* netManager, SchematicItem* item) {
    m_lastNetManager = netManager;
    m_cachedResults = results;
    m_hasCachedResults = true;
    m_cachedItem = item;

    if (!m_initialFitDone) {
        m_initialFitDone = true;
        zoomToFit();
    } else {
        reprocessTraces();
    }
}

static QString cleanNetName(const QString& s) {
    QString t = s.trimmed();
    if (t.startsWith("v(", Qt::CaseInsensitive) && t.endsWith(")")) {
        t = t.mid(2, t.length() - 3).trimmed();
    }
    return t;
}

void OscilloscopeWindow::autoScaleChannels() {
    int count = m_config.channelCount;
    bool changed = false;

    if (m_hasCachedResults) {
        for (int i = 0; i < count && i < m_config.channels.size(); ++i) {
            if (!m_config.channels[i].enabled) continue;

            QString posNet, negNet;
            if (m_cachedItem && m_lastNetManager) {
                const auto pts = m_cachedItem->connectionPoints();
                if (i < pts.size()) posNet = m_lastNetManager->findNetAtPoint(m_cachedItem->mapToScene(pts[i]));
                if (count + i < pts.size()) negNet = m_lastNetManager->findNetAtPoint(m_cachedItem->mapToScene(pts[count + i]));
            }

            if (posNet.isEmpty()) {
                continue;
            }

            auto matchWave = [](const SimWaveform& wave, const QString& targetNet) -> bool {
                if (targetNet.isEmpty()) return false;
                const QString wName = QString::fromStdString(wave.name);
                const QString cTarget = cleanNetName(targetNet);
                const QString cWave = cleanNetName(wName);

                if (cTarget.compare(cWave, Qt::CaseInsensitive) == 0) return true;
                if (wName.compare(targetNet, Qt::CaseInsensitive) == 0) return true;
                if (wName.compare("V(" + targetNet + ")", Qt::CaseInsensitive) == 0) return true;
                if (wName.endsWith("(" + targetNet + ")", Qt::CaseInsensitive)) return true;
                return false;
            };

            const SimWaveform* pWave = nullptr;
            const SimWaveform* nWave = nullptr;

            for (const auto& wave : m_cachedResults.waveforms) {
                if (!pWave && matchWave(wave, posNet)) pWave = &wave;
                else if (!nWave && matchWave(wave, negNet)) nWave = &wave;
            }

            if (pWave && !pWave->yData.empty()) {
                double minY = pWave->yData[0];
                double maxY = pWave->yData[0];
                bool floating = m_config.channels[i].floatingGround;

                for (size_t s = 0; s < pWave->yData.size(); ++s) {
                    double v = pWave->yData[s];
                    if (floating && nWave && s < nWave->yData.size()) v -= nWave->yData[s];
                    minY = std::min(minY, v);
                    maxY = std::max(maxY, v);
                }

                double vpp = std::max(1e-4, maxY - minY);
                double idealVdiv = vpp / 6.0;

                double exponent = std::floor(std::log10(idealVdiv));
                double mantissa = idealVdiv / std::pow(10.0, exponent);
                double standardMantissa = 1.0;
                if (mantissa > 5.0) standardMantissa = 10.0;
                else if (mantissa > 2.0) standardMantissa = 5.0;
                else if (mantissa > 1.0) standardMantissa = 2.0;

                double niceVdiv = standardMantissa * std::pow(10.0, exponent);
                niceVdiv = std::clamp(niceVdiv, 0.001, 1000.0);

                double midV = (minY + maxY) / 2.0;
                double idealOffsetDivs = -midV / niceVdiv;

                m_config.channels[i].scale = 1.0 / niceVdiv;
                m_config.channels[i].offset = idealOffsetDivs;
                changed = true;

                if (i < m_channelUIs.size()) {
                    if (m_channelUIs[i].voltsDiv) {
                        m_channelUIs[i].voltsDiv->blockSignals(true);
                        m_channelUIs[i].voltsDiv->setValue(niceVdiv);
                        m_channelUIs[i].voltsDiv->blockSignals(false);
                    }
                    if (m_channelUIs[i].offset) {
                        m_channelUIs[i].offset->blockSignals(true);
                        m_channelUIs[i].offset->setValue(idealOffsetDivs);
                        m_channelUIs[i].offset->blockSignals(false);
                    }
                }
            }
        }
    } else if (m_scopeDisplay) {
        // Live simulation mode: compute directly from m_scopeDisplay trace stats
        for (int i = 0; i < count && i < m_config.channels.size(); ++i) {
            if (!m_config.channels[i].enabled) continue;
            QString chKey = QString("CH%1").arg(i + 1);
            auto stats = m_scopeDisplay->getTraceStats(chKey);
            if (stats.hasData && stats.maxV > stats.minV) {
                double vpp = std::max(1e-4, stats.maxV - stats.minV);
                double idealVdiv = vpp / 6.0;

                double exponent = std::floor(std::log10(idealVdiv));
                double mantissa = idealVdiv / std::pow(10.0, exponent);
                double standardMantissa = 1.0;
                if (mantissa > 5.0) standardMantissa = 10.0;
                else if (mantissa > 2.0) standardMantissa = 5.0;
                else if (mantissa > 1.0) standardMantissa = 2.0;

                double niceVdiv = standardMantissa * std::pow(10.0, exponent);
                niceVdiv = std::clamp(niceVdiv, 0.001, 1000.0);

                double midV = (stats.minV + stats.maxV) / 2.0;
                double idealOffsetDivs = -midV / niceVdiv;

                m_config.channels[i].scale = 1.0 / niceVdiv;
                m_config.channels[i].offset = idealOffsetDivs;
                changed = true;

                if (i < m_channelUIs.size()) {
                    if (m_channelUIs[i].voltsDiv) {
                        m_channelUIs[i].voltsDiv->blockSignals(true);
                        m_channelUIs[i].voltsDiv->setValue(niceVdiv);
                        m_channelUIs[i].voltsDiv->blockSignals(false);
                    }
                    if (m_channelUIs[i].offset) {
                        m_channelUIs[i].offset->blockSignals(true);
                        m_channelUIs[i].offset->setValue(idealOffsetDivs);
                        m_channelUIs[i].offset->blockSignals(false);
                    }
                }
            }
        }
    }

    if (changed) {
        Q_EMIT configChanged(m_itemId, m_config);
    }
    if (m_hasCachedResults) {
        reprocessTraces();
    }
}

void OscilloscopeWindow::fitYAxis() {
    autoScaleChannels();
}

void OscilloscopeWindow::zoomToFit() {
    // 1. Auto-scale channel vertical ranges
    autoScaleChannels();

    // 2. Auto-scale timebase based on simulation waveform duration or active traces
    double maxSpan = 0.0;
    if (m_hasCachedResults) {
        for (const auto& wave : m_cachedResults.waveforms) {
            if (wave.xData.size() > 1) {
                double tSpan = wave.xData.back() - wave.xData.front();
                if (tSpan > maxSpan) maxSpan = tSpan;
            }
        }
    } else if (m_scopeDisplay) {
        auto overall = m_scopeDisplay->getOverallTraceStats();
        if (overall.hasData && overall.maxT > overall.minT) {
            maxSpan = overall.maxT - overall.minT;
        }
    }

    if (maxSpan > 1e-12) {
        double idealTdiv = maxSpan / 10.0;
        double exp = std::floor(std::log10(idealTdiv));
        double mant = idealTdiv / std::pow(10.0, exp);
        double stdMant = 1.0;
        if (mant > 5.0) stdMant = 10.0;
        else if (mant > 2.0) stdMant = 5.0;
        else if (mant > 1.0) stdMant = 2.0;

        double niceTdiv = stdMant * std::pow(10.0, exp);
        niceTdiv = std::clamp(niceTdiv, 1e-9, 10.0);

        m_config.timebase = niceTdiv;
        if (m_timebaseSpin) {
            m_timebaseSpin->blockSignals(true);
            m_timebaseSpin->setValue(niceTdiv);
            m_timebaseSpin->blockSignals(false);
        }
        if (m_scopeDisplay) {
            m_scopeDisplay->setTimebase(niceTdiv);
        }
        Q_EMIT configChanged(m_itemId, m_config);
    }

    if (m_hasCachedResults) {
        reprocessTraces();
    }
}

void OscilloscopeWindow::reprocessTraces() {
    if (!m_hasCachedResults || !m_scopeDisplay) return;

    QMap<QString, QVector<QPointF>> visibleTraces;
    QMap<QString, QColor> traceColors;
    QJsonObject remoteData;
    QJsonArray remoteTraces;
    int count = m_config.channelCount;
    
    for (int i = 0; i < count && i < m_config.channels.size(); ++i) {
        traceColors[QString("CH%1").arg(i + 1)] = m_config.channels[i].color;
        if (!m_config.channels[i].enabled) continue;
        
        QString posNet, negNet;
        if (m_cachedItem && m_lastNetManager) {
            const auto pts = m_cachedItem->connectionPoints();
            if (i < pts.size()) {
                posNet = m_lastNetManager->findNetAtPoint(m_cachedItem->mapToScene(pts[i]));
            }
            if (count + i < pts.size()) {
                negNet = m_lastNetManager->findNetAtPoint(m_cachedItem->mapToScene(pts[count + i]));
            }
        }

        if (posNet.isEmpty()) {
            continue;
        }

        auto matchWave = [](const SimWaveform& wave, const QString& targetNet) -> bool {
            if (targetNet.isEmpty()) return false;
            const QString wName = QString::fromStdString(wave.name);
            const QString cTarget = cleanNetName(targetNet);
            const QString cWave = cleanNetName(wName);

            if (cTarget.compare(cWave, Qt::CaseInsensitive) == 0) return true;
            if (wName.compare(targetNet, Qt::CaseInsensitive) == 0) return true;
            if (wName.compare("V(" + targetNet + ")", Qt::CaseInsensitive) == 0) return true;
            if (wName.endsWith("(" + targetNet + ")", Qt::CaseInsensitive)) return true;
            return false;
        };

        const SimWaveform* pWave = nullptr;
        const SimWaveform* nWave = nullptr;

        for (const auto& wave : m_cachedResults.waveforms) {
            if (!pWave && matchWave(wave, posNet)) {
                pWave = &wave;
            } else if (!nWave && matchWave(wave, negNet)) {
                nWave = &wave;
            }
        }
        
        if (pWave) {
            // Min/max-bucket decimation to a fixed budget: a full-rate
            // polyline costs ~16B/point (~160MB/channel at 10M points) and
            // freezes the UI, while naive striding drops narrow spikes.
            // Min/max preserves the envelope. Iterate to min(x,y) length —
            // ragged waves must not index past yData.
            const size_t total = std::min(pWave->xData.size(), pWave->yData.size());
            constexpr size_t kBuckets = 500;

            QVector<QPointF> points;
            points.reserve((qsizetype)std::min<size_t>(total, 2 * kBuckets));

            QJsonArray xArray;
            QJsonArray yArray;

            double scale = m_config.channels[i].scale;
            double offset = m_config.channels[i].offset;
            bool floating = m_config.channels[i].floatingGround;

            auto pushPoint = [&](size_t s) {
                double x = pWave->xData[s];
                double v = pWave->yData[s];
                if (floating && nWave && s < nWave->yData.size()) {
                    v -= nWave->yData[s];
                }
                double y = (v * scale) + offset;
                points.append(QPointF(x, y));
                xArray.append(x);
                yArray.append(y);
            };

            if (total <= 2 * kBuckets) {
                for (size_t s = 0; s < total; ++s) pushPoint(s);
            } else {
                for (size_t b = 0; b < kBuckets; ++b) {
                    const size_t b0 = b * total / kBuckets;
                    const size_t b1 = (b + 1) * total / kBuckets;
                    if (b1 <= b0) continue;
                    size_t iMin = b0, iMax = b0;
                    double yMin = 0.0, yMax = 0.0;
                    for (size_t s = b0; s < b1; ++s) {
                        double v = pWave->yData[s];
                        if (floating && nWave && s < nWave->yData.size()) v -= nWave->yData[s];
                        const double y = (v * scale) + offset;
                        if (s == b0 || y < yMin) { yMin = y; iMin = s; }
                        if (s == b0 || y > yMax) { yMax = y; iMax = s; }
                    }
                    // Emit in x order so the polyline never zig-zags.
                    if (iMin == iMax) pushPoint(iMin);
                    else if (iMin < iMax) { pushPoint(iMin); pushPoint(iMax); }
                    else { pushPoint(iMax); pushPoint(iMin); }
                }
            }
            visibleTraces[QString("CH%1").arg(i+1)] = points;

            QJsonObject traceObj;
            traceObj["channel"] = i + 1;
            traceObj["x"] = xArray;
            traceObj["y"] = yArray;
            traceObj["color"] = m_config.channels[i].color.name();
            remoteTraces.append(traceObj);
        }
    }
    
    m_scopeDisplay->setMultiTraceData(visibleTraces, traceColors);

    // Broadcast to remote clients
    remoteData["traces"] = remoteTraces;
    remoteData["timebase"] = m_config.timebase;
    remoteData["name"] = m_itemName;
    
    static qint64 lastBroadcast = 0;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - lastBroadcast > 50) { // Max 20fps for remote
#ifdef VIOSPICE_HAS_REMOTE_DISPLAY
        RemoteDisplayServer::instance().broadcastUpdate("oscilloscope", m_itemId, remoteData);
#endif
        lastBroadcast = now;
    }
}

void OscilloscopeWindow::clear() {
    m_hasCachedResults = false;
    m_scopeDisplay->clear();
}

QImage OscilloscopeWindow::renderToImage(const QSize& size) {
    return m_scopeDisplay->renderToImage(size);
}

void OscilloscopeWindow::closeEvent(QCloseEvent* event) {
    Q_EMIT windowClosing(m_itemId);
    event->accept();
}

void OscilloscopeWindow::onChannelToggled(int ch, bool checked) {
    if (ch >= 0 && ch < m_config.channels.size()) {
        m_config.channels[ch].enabled = checked;
        reprocessTraces();
        Q_EMIT configChanged(m_itemId, m_config);
    }
}

void OscilloscopeWindow::onFloatingToggled(int ch, bool checked) {
    if (ch >= 0 && ch < m_config.channels.size()) {
        m_config.channels[ch].floatingGround = checked;
        reprocessTraces();
        Q_EMIT configChanged(m_itemId, m_config);
    }
}

void OscilloscopeWindow::onTimebaseChanged(double value) {
    m_config.timebase = value;
    if (m_scopeDisplay) {
        m_scopeDisplay->setTimebase(value);
    }
    reprocessTraces();
    Q_EMIT configChanged(m_itemId, m_config);
}

void OscilloscopeWindow::onVoltsDivChanged(int ch, double value) {
    if (value > 0 && ch >= 0 && ch < m_config.channels.size()) {
        m_config.channels[ch].scale = 1.0 / value;
        reprocessTraces();
        Q_EMIT configChanged(m_itemId, m_config);
    }
}

void OscilloscopeWindow::onOffsetChanged(int ch, double value) {
    if (ch >= 0 && ch < m_config.channels.size()) {
        m_config.channels[ch].offset = value;
        reprocessTraces();
        Q_EMIT configChanged(m_itemId, m_config);
    }
}

void OscilloscopeWindow::onTriggerSourceChanged(int index) {
    m_config.triggerSource = QString("CH%1").arg(index + 1);
    reprocessTraces();
    Q_EMIT configChanged(m_itemId, m_config);
}

void OscilloscopeWindow::onTriggerLevelChanged(double value) {
    m_config.triggerLevel = value;
    reprocessTraces();
    Q_EMIT configChanged(m_itemId, m_config);
}

void OscilloscopeWindow::onFreezeClicked() {
    if (m_scopeDisplay) {
        m_scopeDisplay->freezeCurrentTraces();
    }
}

void OscilloscopeWindow::onClearMemoriesClicked() {
    if (m_scopeDisplay) {
        m_scopeDisplay->clearMemories();
    }
}
