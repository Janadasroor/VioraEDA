#ifndef BV_MOS_SYNTHESIZER_H
#define BV_MOS_SYNTHESIZER_H

#include "mos_synthesizer_interface.h"

/**
 * @brief Behavioral Voltage & MOS Buffer Synthesizer.
 * Creates a subcircuit utilizing a Behavioral/PWL source to generate the 
 * time-domain sequence and buffers it through basic MOS devices.
 */
class BVMosSynthesizer : public IMosWaveformSynthesizer {
public:
    BVMosSynthesizer() = default;
    
    QString synthesizeNetlist(const QString& subcktName,
                              const QVector<QPointF>& points, 
                              const QMap<QString, QString>& params) const override;
                              
    QList<ParameterInfo> getRequiredParameters() const override;
    
    QString getTopologyName() const override {
        return "BV Source + MOS Buffer";
    }
};

#endif // BV_MOS_SYNTHESIZER_H
