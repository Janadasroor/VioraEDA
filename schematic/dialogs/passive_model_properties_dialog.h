#ifndef PASSIVE_MODEL_PROPERTIES_DIALOG_H
#define PASSIVE_MODEL_PROPERTIES_DIALOG_H

#include <QDialog>

class QLineEdit;
class QCheckBox;
class QGroupBox;
class SchematicItem;

class PassiveModelPropertiesDialog : public QDialog {
    Q_OBJECT

public:
    enum class Kind {
        Resistor,
        Capacitor,
        Inductor
    };

    explicit PassiveModelPropertiesDialog(SchematicItem* item, Kind kind, QWidget* parent = nullptr);

    QString reference() const;
    QString valueText() const;
    QString spiceModel() const;
    QString manufacturer() const;
    QString mpn() const;
    bool excludeFromSimulation() const;
    bool excludeFromPcb() const;

private Q_SLOTS:
    void pickModel();

private:
    SchematicItem* m_item;
    Kind m_kind;
    QLineEdit* m_referenceEdit = nullptr;
    QLineEdit* m_valueEdit = nullptr;
    QLineEdit* m_spiceModelEdit = nullptr;
    QLineEdit* m_manufacturerEdit = nullptr;
    QLineEdit* m_mpnEdit = nullptr;
    QGroupBox* m_inductorModelGroup = nullptr;
    QLineEdit* m_seriesResistanceEdit = nullptr;
    QLineEdit* m_parallelResistanceEdit = nullptr;
    QLineEdit* m_parallelCapacitanceEdit = nullptr;
    QLineEdit* m_initialCurrentEdit = nullptr;
    QCheckBox* m_excludeSimCheck = nullptr;
    QCheckBox* m_excludePcbCheck = nullptr;
};

#endif // PASSIVE_MODEL_PROPERTIES_DIALOG_H
