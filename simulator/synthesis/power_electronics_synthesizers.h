#ifndef POWER_ELECTRONICS_SYNTHESIZERS_H
#define POWER_ELECTRONICS_SYNTHESIZERS_H

#include "mos_synthesizer_interface.h"

// All-NMOS Half-Bridge Synthesizer
class HalfBridgeSynthesizer : public IMosWaveformSynthesizer {
public:
    QString synthesizeNetlist(const QString& subcktName,
                              const QVector<QPointF>& points, 
                              const QMap<QString, QString>& params) const override;
    QList<ParameterInfo> getRequiredParameters() const override;
    QString getTopologyName() const override { return "All-NMOS Half-Bridge (Power)"; }
};

// All-NMOS Push-Pull Synthesizer
class PushPullSynthesizer : public IMosWaveformSynthesizer {
public:
    QString synthesizeNetlist(const QString& subcktName,
                              const QVector<QPointF>& points, 
                              const QMap<QString, QString>& params) const override;
    QList<ParameterInfo> getRequiredParameters() const override;
    QString getTopologyName() const override { return "All-NMOS Push-Pull (Power)"; }
};

// NMOS Matrix Switch Synthesizer
class MatrixSynthesizer : public IMosWaveformSynthesizer {
public:
    QString synthesizeNetlist(const QString& subcktName,
                              const QVector<QPointF>& points, 
                              const QMap<QString, QString>& params) const override;
    QList<ParameterInfo> getRequiredParameters() const override;
    QString getTopologyName() const override { return "NMOS Matrix Switch (Bidirectional)"; }
};

#endif // POWER_ELECTRONICS_SYNTHESIZERS_H