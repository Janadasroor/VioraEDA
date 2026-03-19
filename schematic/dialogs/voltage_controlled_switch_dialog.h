#ifndef VOLTAGE_CONTROLLED_SWITCH_DIALOG_H
#define VOLTAGE_CONTROLLED_SWITCH_DIALOG_H

#include <QDialog>
#include <QPointer>

class QLineEdit;
class VoltageControlledSwitchItem;

class VoltageControlledSwitchDialog : public QDialog {
    Q_OBJECT
public:
    explicit VoltageControlledSwitchDialog(VoltageControlledSwitchItem* item, QWidget* parent = nullptr);

private:
    void applyChanges();

    QPointer<VoltageControlledSwitchItem> m_item;
    QLineEdit* m_modelName = nullptr;
    QLineEdit* m_ron = nullptr;
    QLineEdit* m_roff = nullptr;
    QLineEdit* m_vt = nullptr;
    QLineEdit* m_vh = nullptr;
};

#endif // VOLTAGE_CONTROLLED_SWITCH_DIALOG_H
