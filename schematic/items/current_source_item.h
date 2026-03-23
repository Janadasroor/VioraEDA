#ifndef CURRENTSOURCEITEM_H
#define CURRENTSOURCEITEM_H

#include "schematic_item.h"
#include "schematic_primitives.h"
#include "../../symbols/models/symbol_definition.h"
#include "../../symbols/items/symbol_primitive_item.h"
#include <QBrush>
#include <QPen>
#include <memory>
#include <vector>

using Flux::Model::SymbolDefinition;
using Flux::Model::SymbolPrimitive;
using namespace Flux::Item;

class CurrentSourceItem : public SchematicItem {
public:
    enum SourceType { 
        DC, 
        Sine, 
        Pulse, 
        EXP,     // Exponential
        SFFM,    // Single Frequency FM
        PWL,     // Piecewise Linear
        PWLFile, // PWL from file
        Noise,   // Random Noise
        Behavioral, // Arbitrary Behavioral Source (BI)
        AC = Sine // Legacy compatibility
    };

    CurrentSourceItem(QPointF pos = QPointF(), const QString& value = "1mA", SourceType type = DC, QGraphicsItem* parent = nullptr);

    QString itemTypeName() const override;
    ItemType itemType() const override { return SchematicItem::CurrentSourceType; }
    QString referencePrefix() const override { return (m_sourceType == Behavioral) ? "B" : "I"; }
    void rebuildPrimitives() override;
    QJsonObject toJson() const override;
    bool fromJson(const QJsonObject& json) override;
    SchematicItem* clone() const override;

    void onInteractiveDoubleClick(const QPointF& pos) override;
    void updateLabelText();

    QList<QPointF> connectionPoints() const override;
    QList<PinElectricalType> pinElectricalTypes() const override { return { PassivePin, PassivePin }; }

    QString value() const override { return m_value; }
    void setValue(const QString& val) override;

    SourceType sourceType() const { return m_sourceType; }
    void setSourceType(SourceType type);

    QString dcCurrent() const { return m_dcCurrent; }
    void setDcCurrent(const QString& v) { m_dcCurrent = v; updateValue(); update(); }

    // Sine Params
    QString sineAmplitude() const { return m_sineAmplitude; }
    void setSineAmplitude(const QString& v) { m_sineAmplitude = v; updateValue(); update(); }
    QString sineFrequency() const { return m_sineFrequency; }
    void setSineFrequency(const QString& f) { m_sineFrequency = f; updateValue(); update(); }
    QString sineOffset() const { return m_sineOffset; }
    void setSineOffset(const QString& o) { m_sineOffset = o; updateValue(); update(); }
    QString sineDelay() const { return m_sineDelay; }
    void setSineDelay(const QString& d) { m_sineDelay = d; updateValue(); update(); }
    QString sineTheta() const { return m_sineTheta; }
    void setSineTheta(const QString& t) { m_sineTheta = t; updateValue(); update(); }
    QString sinePhi() const { return m_sinePhi; }
    void setSinePhi(const QString& p) { m_sinePhi = p; updateValue(); update(); }
    QString sineNcycles() const { return m_sineNcycles; }
    void setSineNcycles(const QString& n) { m_sineNcycles = n; updateValue(); update(); }

    // Pulse Params
    QString pulseI1() const { return m_pulseI1; }
    void setPulseI1(const QString& v) { m_pulseI1 = v; updateValue(); update(); }
    QString pulseI2() const { return m_pulseI2; }
    void setPulseI2(const QString& v) { m_pulseI2 = v; updateValue(); update(); }
    QString pulseDelay() const { return m_pulseDelay; }
    void setPulseDelay(const QString& d) { m_pulseDelay = d; updateValue(); update(); }
    QString pulseRise() const { return m_pulseRise; }
    void setPulseRise(const QString& r) { m_pulseRise = r; updateValue(); update(); }
    QString pulseFall() const { return m_pulseFall; }
    void setPulseFall(const QString& f) { m_pulseFall = f; updateValue(); update(); }
    QString pulseWidth() const { return m_pulseWidth; }
    void setPulseWidth(const QString& w) { m_pulseWidth = w; updateValue(); update(); }
    QString pulsePeriod() const { return m_pulsePeriod; }
    void setPulsePeriod(const QString& p) { m_pulsePeriod = p; updateValue(); update(); }
    QString pulseNcycles() const { return m_pulseNcycles; }
    void setPulseNcycles(const QString& n) { m_pulseNcycles = n; updateValue(); update(); }

    // EXP Params
    QString expI1() const { return m_expI1; }
    void setExpI1(const QString& v) { m_expI1 = v; updateValue(); update(); }
    QString expI2() const { return m_expI2; }
    void setExpI2(const QString& v) { m_expI2 = v; updateValue(); update(); }
    QString expTd1() const { return m_expTd1; }
    void setExpTd1(const QString& t) { m_expTd1 = t; updateValue(); update(); }
    QString expTau1() const { return m_expTau1; }
    void setExpTau1(const QString& t) { m_expTau1 = t; updateValue(); update(); }
    QString expTd2() const { return m_expTd2; }
    void setExpTd2(const QString& t) { m_expTd2 = t; updateValue(); update(); }
    QString expTau2() const { return m_expTau2; }
    void setExpTau2(const QString& t) { m_expTau2 = t; updateValue(); update(); }

    // SFFM Params
    QString sffmOff() const { return m_sffmOff; }
    void setSffmOff(const QString& v) { m_sffmOff = v; updateValue(); update(); }
    QString sffmAmplit() const { return m_sffmAmplit; }
    void setSffmAmplit(const QString& v) { m_sffmAmplit = v; updateValue(); update(); }
    QString sffmCarrier() const { return m_sffmCarrier; }
    void setSffmCarrier(const QString& f) { m_sffmCarrier = f; updateValue(); update(); }
    QString sffmModIndex() const { return m_sffmModIndex; }
    void setSffmModIndex(const QString& i) { m_sffmModIndex = i; updateValue(); update(); }
    QString sffmSignalFreq() const { return m_sffmSignalFreq; }
    void setSffmSignalFreq(const QString& f) { m_sffmSignalFreq = f; updateValue(); update(); }

    // PWL Params
    QString pwlPoints() const { return m_pwlPoints; }
    void setPwlPoints(const QString& p) { m_pwlPoints = p; updateValue(); update(); }
    QString pwlFile() const { return m_pwlFile; }
    void setPwlFile(const QString& f) { m_pwlFile = f; updateValue(); update(); }
    bool pwlRepeat() const { return m_pwlRepeat; }
    void setPwlRepeat(bool r) { m_pwlRepeat = r; updateValue(); update(); }

    ~CurrentSourceItem() override;

    QString acAmplitude() const { return m_acAmplitude; }
    void setAcAmplitude(const QString& v) { m_acAmplitude = v; updateValue(); update(); }
    QString acPhase() const { return m_acPhase; }
    void setAcPhase(const QString& p) { m_acPhase = p; updateValue(); update(); }

    QString seriesResistance() const { return m_seriesResistance; }
    void setSeriesResistance(const QString& r) { m_seriesResistance = r; updateValue(); update(); }
    QString parallelCapacitance() const { return m_parallelCapacitance; }
    void setParallelCapacitance(const QString& c) { m_parallelCapacitance = c; updateValue(); update(); }

    bool isFunctionVisible() const { return m_showFunction; }
    void setFunctionVisible(bool v) { m_showFunction = v; update(); }
    bool isDcVisible() const { return m_showDc; }
    void setDcVisible(bool v) { m_showDc = v; update(); }
    bool isAcVisible() const { return m_showAc; }
    void setAcVisible(bool v) { m_showAc = v; update(); }
    bool isParasiticVisible() const { return m_showParasitic; }
    void setParasiticVisible(bool v) { m_showParasitic = v; update(); }

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    void updateValue();
    void rebuildExternalSymbol();
    void clearExternalSymbolItems();
    QList<SymbolPrimitive> resolvedExternalPrimitives() const;

    SourceType m_sourceType;
    QString m_dcCurrent;
    
    QString m_sineAmplitude;
    QString m_sineFrequency;
    QString m_sineOffset;
    QString m_sineDelay;
    QString m_sineTheta;
    QString m_sinePhi;
    QString m_sineNcycles;
    
    QString m_pulseI1;
    QString m_pulseI2;
    QString m_pulseDelay;
    QString m_pulseRise;
    QString m_pulseFall;
    QString m_pulseWidth;
    QString m_pulsePeriod;
    QString m_pulseNcycles;

    QString m_expI1;
    QString m_expI2;
    QString m_expTd1;
    QString m_expTau1;
    QString m_expTd2;
    QString m_expTau2;

    QString m_sffmOff;
    QString m_sffmAmplit;
    QString m_sffmCarrier;
    QString m_sffmModIndex;
    QString m_sffmSignalFreq;

    QString m_pwlPoints;
    QString m_pwlFile;
    bool m_pwlRepeat;

    QString m_acAmplitude;
    QString m_acPhase;

    QString m_seriesResistance;
    QString m_parallelCapacitance;

    bool m_showFunction;
    bool m_showDc;
    bool m_showAc;
    bool m_showParasitic;

    QPen m_pen;
    QBrush m_brush;
    std::vector<std::unique_ptr<SchematicPrimitive>> m_primitives;
    bool m_useExternalSymbol = false;
    SymbolDefinition m_externalSymbol;
    QList<SymbolPrimitiveItem*> m_symbolItems;
    QRectF m_externalBounds;
};

#endif // CURRENTSOURCEITEM_H
