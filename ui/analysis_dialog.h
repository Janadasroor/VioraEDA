// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Janada Sroor

#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QGridLayout>

class AnalysisDialog : public QDialog {
    Q_OBJECT
public:
    explicit AnalysisDialog(QWidget *parent = nullptr);
    void setValues(const QString &seriesName, const QString &tStart, const QString &tEnd, const QString &average, const QString &rms_or_integral, bool isPower);
    void setAcValues(const QString &seriesName, const QString &startFreq, const QString &endFreq, const QString &reference, const QString &bw, const QString &powerBw);
    void setEdgeTimes(int riseCount, const QString &riseMin, const QString &riseAvg, const QString &riseMax,
                      int fallCount, const QString &fallMin, const QString &fallAvg, const QString &fallMax);
    void setSignalProperties(const QString &vpp, const QString &period, const QString &frequency,
                             const QString &dutyCycle, const QString &pulseWidth,
                             const QString &overshoot, const QString &undershoot);

private:
    QLabel *m_titleLabel;
    QLabel *m_startLabel;
    QLabel *m_endLabel;
    QLabel *m_avgLabel;
    QLineEdit *m_tStart;
    QLineEdit *m_tEnd;
    QLineEdit *m_average;
    QLineEdit *m_rmsIntegral;
    QLabel *m_rmsIntegralLabel;
    QLabel *m_bwLabel;
    QLineEdit *m_bw;
    QLabel *m_powerBwLabel;
    QLineEdit *m_powerBw;

    QLabel *m_edgeSepLabel;
    QLabel *m_riseLabel;
    QLineEdit *m_riseMin;
    QLineEdit *m_riseAvg;
    QLineEdit *m_riseMax;
    QLabel *m_fallLabel;
    QLineEdit *m_fallMin;
    QLineEdit *m_fallAvg;
    QLineEdit *m_fallMax;

    QLabel *m_sigSepLabel;
    QLabel *m_vppLabel;
    QLineEdit *m_vpp;
    QLabel *m_periodLabel;
    QLineEdit *m_period;
    QLabel *m_freqLabel;
    QLineEdit *m_freq;
    QLabel *m_dutyLabel;
    QLineEdit *m_duty;
    QLabel *m_pulseLabel;
    QLineEdit *m_pulse;
    QLabel *m_overshootLabel;
    QLineEdit *m_overshoot;
    QLabel *m_undershootLabel;
    QLineEdit *m_undershoot;

    QLineEdit* createReadOnlyEdit();
};
