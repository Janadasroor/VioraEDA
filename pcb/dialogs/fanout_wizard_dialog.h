#ifndef FANOUT_WIZARD_DIALOG_H
#define FANOUT_WIZARD_DIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QGraphicsScene>
#include "../items/component_item.h"

class FanoutWizardDialog : public QDialog {
    Q_OBJECT

public:
    struct FanoutOptions {
        enum Direction { Inward, Outward, Alternating };
        Direction direction = Outward;
        double traceWidth = 0.25;
        double viaDiameter = 0.6;
        double viaDrill = 0.3;
        double shortTraceLength = 1.27;
        bool onlyPowerNets = false;
        bool includeUnconnected = true;
    };

    explicit FanoutWizardDialog(ComponentItem* component, QWidget* parent = nullptr);
    ~FanoutWizardDialog() = default;

    FanoutOptions options() const;

private:
    void setupUI();
    
    ComponentItem* m_component;
    
    QComboBox* m_directionCombo;
    QDoubleSpinBox* m_widthSpin;
    QDoubleSpinBox* m_viaDiaSpin;
    QDoubleSpinBox* m_viaDrillSpin;
    QDoubleSpinBox* m_lengthSpin;
    QCheckBox* m_powerOnlyCheck;
    QCheckBox* m_unconnectedCheck;
};

#endif // FANOUT_WIZARD_DIALOG_H
