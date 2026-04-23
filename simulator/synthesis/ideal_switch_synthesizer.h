#ifndef IDEAL_SWITCH_SYNTHESIZER_H
#define IDEAL_SWITCH_SYNTHESIZER_H

#include "mos_synthesizer_interface.h"

/**
 * @brief High-performance Behavioral Switch Synthesizer.
 * Replaces actual MOSFETs with B-Source behavioral resistors for 
 * superior convergence in fast-switching power stage simulations.
 */
class IdealSwitchSynthesizer : public IMosWaveformSynthesizer {
public:
    IdealSwitchSynthesizer() = default;
    
    QString synthesizeNetlist(const QString& subcktName,
                              const QVector<QPointF>& points, 
                              const QMap<QString, QString>& params) const override;
                              
    QList<ParameterInfo> getRequiredParameters() const override;
    
    QString getTopologyName() const override {
        return "Ideal Behavioral Switch (B-Source)";
    }
};

#endif // IDEAL_SWITCH_SYNTHESIZER_H
