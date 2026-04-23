#ifndef MOS_SYNTHESIZER_INTERFACE_H
#define MOS_SYNTHESIZER_INTERFACE_H

#include <QString>
#include <QVector>
#include <QPointF>
#include <QMap>
#include <QList>

struct ParameterInfo {
    QString name;
    QString defaultValue;
    QString description;
    QString unit;
};

class IMosWaveformSynthesizer {
public:
    virtual ~IMosWaveformSynthesizer() = default;

    /**
     * @brief Synthesize a SPICE .subckt netlist matching the provided drawn points.
     * @param subcktName Name of the generated subcircuit
     * @param points Drawn waveform points (Time vs Voltage)
     * @param params User-specified parameters to tune the output characteristics
     * @return Raw SPICE .subckt string
     */
    virtual QString synthesizeNetlist(const QString& subcktName,
                                      const QVector<QPointF>& points, 
                                      const QMap<QString, QString>& params) const = 0;

    /**
     * @brief Get a list of parameters the user can adjust in the UI for this specific topology
     */
    virtual QList<ParameterInfo> getRequiredParameters() const = 0;
    
    /**
     * @brief Display name defining the topology in the UI dropdown
     */
    virtual QString getTopologyName() const = 0;
};

#endif // MOS_SYNTHESIZER_INTERFACE_H
