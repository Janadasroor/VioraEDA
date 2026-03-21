// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Janada Sroor

#include "analysis_dialog.h"
#include <QHBoxLayout>
#include <cmath>

AnalysisDialog::AnalysisDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Waveform Analysis");
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    
    auto *layout = new QGridLayout(this);
    
    m_titleLabel = new QLabel("Waveform: ");
    layout->addWidget(m_titleLabel, 0, 0, 1, 2);
    
    m_startLabel = new QLabel("Interval Start:");
    layout->addWidget(m_startLabel, 1, 0);
    m_tStart = createReadOnlyEdit();
    layout->addWidget(m_tStart, 1, 1);
    
    m_endLabel = new QLabel("Interval End:");
    layout->addWidget(m_endLabel, 2, 0);
    m_tEnd = createReadOnlyEdit();
    layout->addWidget(m_tEnd, 2, 1);
    
    m_avgLabel = new QLabel("Average:");
    layout->addWidget(m_avgLabel, 3, 0);
    m_average = createReadOnlyEdit();
    layout->addWidget(m_average, 3, 1);
    
    m_rmsIntegralLabel = new QLabel("RMS:");
    layout->addWidget(m_rmsIntegralLabel, 4, 0);
    m_rmsIntegral = createReadOnlyEdit();
    layout->addWidget(m_rmsIntegral, 4, 1);

    m_bwLabel = new QLabel("BW:");
    layout->addWidget(m_bwLabel, 5, 0);
    m_bw = createReadOnlyEdit();
    layout->addWidget(m_bw, 5, 1);

    m_powerBwLabel = new QLabel("Power BW:");
    layout->addWidget(m_powerBwLabel, 6, 0);
    m_powerBw = createReadOnlyEdit();
    layout->addWidget(m_powerBw, 6, 1);

    m_edgeSepLabel = new QLabel("--- Edge Times (10%-90%) ---");
    layout->addWidget(m_edgeSepLabel, 7, 0, 1, 4);

    m_riseLabel = new QLabel("Rise (min/avg/max):");
    layout->addWidget(m_riseLabel, 8, 0);
    auto *riseLayout = new QHBoxLayout();
    m_riseMin = createReadOnlyEdit(); m_riseMin->setFixedWidth(100);
    m_riseAvg = createReadOnlyEdit(); m_riseAvg->setFixedWidth(100);
    m_riseMax = createReadOnlyEdit(); m_riseMax->setFixedWidth(100);
    riseLayout->addWidget(m_riseMin);
    riseLayout->addWidget(m_riseAvg);
    riseLayout->addWidget(m_riseMax);
    riseLayout->setContentsMargins(0, 0, 0, 0);
    auto *riseWrap = new QWidget(); riseWrap->setLayout(riseLayout);
    layout->addWidget(riseWrap, 8, 1, 1, 3);

    m_fallLabel = new QLabel("Fall (min/avg/max):");
    layout->addWidget(m_fallLabel, 9, 0);
    auto *fallLayout = new QHBoxLayout();
    m_fallMin = createReadOnlyEdit(); m_fallMin->setFixedWidth(100);
    m_fallAvg = createReadOnlyEdit(); m_fallAvg->setFixedWidth(100);
    m_fallMax = createReadOnlyEdit(); m_fallMax->setFixedWidth(100);
    fallLayout->addWidget(m_fallMin);
    fallLayout->addWidget(m_fallAvg);
    fallLayout->addWidget(m_fallMax);
    fallLayout->setContentsMargins(0, 0, 0, 0);
    auto *fallWrap = new QWidget(); fallWrap->setLayout(fallLayout);
    layout->addWidget(fallWrap, 9, 1, 1, 3);

    m_sigSepLabel = new QLabel("--- Signal Properties ---");
    layout->addWidget(m_sigSepLabel, 10, 0, 1, 4);

    m_vppLabel = new QLabel("Peak-to-Peak:");
    layout->addWidget(m_vppLabel, 11, 0);
    m_vpp = createReadOnlyEdit();
    layout->addWidget(m_vpp, 11, 1);

    m_periodLabel = new QLabel("Period:");
    layout->addWidget(m_periodLabel, 12, 0);
    m_period = createReadOnlyEdit();
    layout->addWidget(m_period, 12, 1);

    m_freqLabel = new QLabel("Frequency:");
    layout->addWidget(m_freqLabel, 13, 0);
    m_freq = createReadOnlyEdit();
    layout->addWidget(m_freq, 13, 1);

    m_dutyLabel = new QLabel("Duty Cycle:");
    layout->addWidget(m_dutyLabel, 14, 0);
    m_duty = createReadOnlyEdit();
    layout->addWidget(m_duty, 14, 1);

    m_pulseLabel = new QLabel("Pulse Width:");
    layout->addWidget(m_pulseLabel, 15, 0);
    m_pulse = createReadOnlyEdit();
    layout->addWidget(m_pulse, 15, 1);

    m_overshootLabel = new QLabel("Overshoot:");
    layout->addWidget(m_overshootLabel, 16, 0);
    m_overshoot = createReadOnlyEdit();
    layout->addWidget(m_overshoot, 16, 1);

    m_undershootLabel = new QLabel("Undershoot:");
    layout->addWidget(m_undershootLabel, 17, 0);
    m_undershoot = createReadOnlyEdit();
    layout->addWidget(m_undershoot, 17, 1);

    setStyleSheet(R"(
        QDialog { background-color: #f0f0f0; border: 1px solid #ccc; color: black; min-width: 300px; }
        QLabel { font-weight: bold; color: #333; }
        QLineEdit { background-color: white; color: black; border: 1px solid #aaa; padding: 4px; font-family: 'Consolas', monospace; }
    )");

    m_bwLabel->setVisible(false);
    m_bw->setVisible(false);
    m_powerBwLabel->setVisible(false);
    m_powerBw->setVisible(false);
    m_edgeSepLabel->setVisible(false);
    m_riseLabel->setVisible(false);
    m_riseMin->parentWidget()->setVisible(false);
    m_fallLabel->setVisible(false);
    m_fallMin->parentWidget()->setVisible(false);
    m_sigSepLabel->setVisible(false);
    m_vppLabel->setVisible(false); m_vpp->setVisible(false);
    m_periodLabel->setVisible(false); m_period->setVisible(false);
    m_freqLabel->setVisible(false); m_freq->setVisible(false);
    m_dutyLabel->setVisible(false); m_duty->setVisible(false);
    m_pulseLabel->setVisible(false); m_pulse->setVisible(false);
    m_overshootLabel->setVisible(false); m_overshoot->setVisible(false);
    m_undershootLabel->setVisible(false); m_undershoot->setVisible(false);
}

QLineEdit* AnalysisDialog::createReadOnlyEdit() {
    auto *edit = new QLineEdit(this);
    edit->setReadOnly(true);
    edit->setFixedWidth(180);
    edit->setAlignment(Qt::AlignRight);
    return edit;
}

void AnalysisDialog::setValues(const QString &seriesName, const QString &tStart, const QString &tEnd, const QString &average, const QString &rms_or_integral, bool isPower) {
    m_titleLabel->setText("Waveform: " + seriesName);
    m_tStart->setText(tStart);
    m_tEnd->setText(tEnd);
    m_average->setText(average);
    
    if (isPower) {
        m_rmsIntegralLabel->setText("Integral:");
    } else {
        m_rmsIntegralLabel->setText("RMS:");
    }
    m_rmsIntegral->setText(rms_or_integral);

    // Time-domain labels
    m_startLabel->setText("Interval Start:");
    m_endLabel->setText("Interval End:");
    m_avgLabel->setText("Average:");

    m_bwLabel->setVisible(false);
    m_bw->setVisible(false);
    m_powerBwLabel->setVisible(false);
    m_powerBw->setVisible(false);
    m_edgeSepLabel->setVisible(true);
    m_riseLabel->setVisible(true);
    m_riseMin->parentWidget()->setVisible(true);
    m_fallLabel->setVisible(true);
    m_fallMin->parentWidget()->setVisible(true);
    m_sigSepLabel->setVisible(true);
    m_vppLabel->setVisible(true); m_vpp->setVisible(true);
    m_periodLabel->setVisible(true); m_period->setVisible(true);
    m_freqLabel->setVisible(true); m_freq->setVisible(true);
    m_dutyLabel->setVisible(true); m_duty->setVisible(true);
    m_pulseLabel->setVisible(true); m_pulse->setVisible(true);
    m_overshootLabel->setVisible(true); m_overshoot->setVisible(true);
    m_undershootLabel->setVisible(true); m_undershoot->setVisible(true);
}

void AnalysisDialog::setAcValues(const QString &seriesName, const QString &startFreq, const QString &endFreq, const QString &reference, const QString &bw, const QString &powerBw) {
    m_titleLabel->setText("Waveform: " + seriesName);

    m_startLabel->setText("Start Frequency:");
    m_endLabel->setText("End Frequency:");
    m_avgLabel->setText("Reference:");
    m_rmsIntegralLabel->setText("BW:");

    m_tStart->setText(startFreq);
    m_tEnd->setText(endFreq);
    m_average->setText(reference);
    m_rmsIntegral->setText(bw);

    m_bwLabel->setVisible(false);
    m_bw->setVisible(false);
    m_powerBwLabel->setVisible(true);
    m_powerBw->setVisible(true);
    m_powerBwLabel->setText("Power BW:");
    m_powerBw->setText(powerBw);

    m_edgeSepLabel->setVisible(false);
    m_riseLabel->setVisible(false);
    m_riseMin->parentWidget()->setVisible(false);
    m_fallLabel->setVisible(false);
    m_fallMin->parentWidget()->setVisible(false);
    m_sigSepLabel->setVisible(false);
    m_vppLabel->setVisible(false); m_vpp->setVisible(false);
    m_periodLabel->setVisible(false); m_period->setVisible(false);
    m_freqLabel->setVisible(false); m_freq->setVisible(false);
    m_dutyLabel->setVisible(false); m_duty->setVisible(false);
    m_pulseLabel->setVisible(false); m_pulse->setVisible(false);
    m_overshootLabel->setVisible(false); m_overshoot->setVisible(false);
    m_undershootLabel->setVisible(false); m_undershoot->setVisible(false);
}

void AnalysisDialog::setEdgeTimes(int riseCount, const QString &riseMin, const QString &riseAvg, const QString &riseMax,
                                   int fallCount, const QString &fallMin, const QString &fallAvg, const QString &fallMax) {
    m_edgeSepLabel->setText(QString("--- Edge Times (10%%-90%%) ---  [Rise: %1 edges | Fall: %2 edges]").arg(riseCount).arg(fallCount));
    m_riseMin->setText(riseMin);
    m_riseAvg->setText(riseAvg);
    m_riseMax->setText(riseMax);
    m_fallMin->setText(fallMin);
    m_fallAvg->setText(fallAvg);
    m_fallMax->setText(fallMax);
}

void AnalysisDialog::setSignalProperties(const QString &vpp, const QString &period, const QString &frequency,
                                          const QString &dutyCycle, const QString &pulseWidth,
                                          const QString &overshoot, const QString &undershoot) {
    m_vpp->setText(vpp);
    m_period->setText(period);
    m_freq->setText(frequency);
    m_duty->setText(dutyCycle);
    m_pulse->setText(pulseWidth);
    m_overshoot->setText(overshoot);
    m_undershoot->setText(undershoot);
}
