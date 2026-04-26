#ifndef SMARTSIGNALITEM_H
#define SMARTSIGNALITEM_H

#include "schematic_item.h"
#include <QStringList>

/**
 * @brief A programmable signal block defined by embedded FluxScript logic.
 * Python is the external orchestration layer (vspice API); behavioral sources
 * inside the schematic use FluxScript (LLVM JIT) exclusively.
 */
class SmartSignalItem : public SchematicItem {
public:
    // Behavioral source engine — FluxScript only.
    // Python scripts interact with the simulator via the vspice API, not in-simulation.
    enum class EngineType {
        FluxScript
    };

    SmartSignalItem(QPointF pos = QPointF(0, 0), QGraphicsItem* parent = nullptr);


    // SchematicItem interface
    QString itemTypeName() const override { return "SmartSignalBlock"; }
    ItemType itemType() const override { return SchematicItem::SmartSignalType; }
    QString referencePrefix() const override { return "UB"; }
    
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    
    QJsonObject toJson() const override;
    bool fromJson(const QJsonObject& json) override;
    SchematicItem* clone() const override;

    QList<QPointF> connectionPoints() const override;
    QString pinName(int index) const override;
    
    // Logic specific properties
    QString pythonCode() const { return m_pythonCode; }
    void setPythonCode(const QString& code) { 
        m_pythonCode = code; 
        updateDocstring();
        update(); 
    }

    QString fluxCode() const { return m_fluxCode; }
    void setFluxCode(const QString& code) {
        m_fluxCode = code;
        updateDocstring();
        update();
    }

    QString scriptFile() const { return m_scriptFile; }
    void setScriptFile(const QString& path) {
        m_scriptFile = path;
        update();
    }

    EngineType engineType() const { return m_engineType; }
    void setEngineType(EngineType type) { m_engineType = type; update(); }


    QStringList inputPins() const { return m_inputPins; }
    void setInputPins(const QStringList& pins);

    QStringList outputPins() const { return m_outputPins; }
    void setOutputPins(const QStringList& pins);

    QMap<QString, double> parameters() const { return m_parameters; }
    void setParameter(const QString& name, double value) { m_parameters[name] = value; update(); }
    void setParameters(const QMap<QString, double>& params) { m_parameters = params; update(); }

    struct TestCase {
        double time;
        QMap<QString, double> inputs;
        QMap<QString, double> expectedOutputs;
        QString name;
    };
    QList<TestCase> testCases() const { return m_testCases; }
    void setTestCases(const QList<TestCase>& cases) { m_testCases = cases; }

    struct Snapshot {
        QString name;
        QString code;
        QString timestamp;
    };
    QList<Snapshot> snapshots() const { return m_snapshots; }
    void setSnapshots(const QList<Snapshot>& snapshots) { m_snapshots = snapshots; }

    void setPreviewData(const QVector<QPointF>& points) { m_previewPoints = points; update(); }

    QString docstring() const;

private:
    void updateSize();
    void updateDocstring();

    QString m_pythonCode;
    QString m_fluxCode;
    QString m_scriptFile;
    EngineType m_engineType = EngineType::FluxScript;
    QStringList m_inputPins;
    QStringList m_outputPins;
    QMap<QString, double> m_parameters;
    QList<TestCase> m_testCases;
    QList<Snapshot> m_snapshots;
    QSizeF m_size;
    QVector<QPointF> m_previewPoints;
};

#endif // SMARTSIGNALITEM_H
