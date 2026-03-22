#include "schematic_net_label_tool.h"
#include "schematic_view.h"
#include "schematic_commands.h"
#include "../items/net_label_item.h"
#include "../items/hierarchical_port_item.h"
#include <QInputDialog>
#include <QDialog>
#include <QFormLayout>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGraphicsScene>

SchematicNetLabelTool::SchematicNetLabelTool(NetLabelItem::LabelScope scope, QObject* parent)
    : SchematicTool(scope == NetLabelItem::Global ? "Global Label" : "Net Label", parent)
    , m_scope(scope) {
}

NetLabelDialogResult promptNetLabelDialog(QWidget* parent,
                                         const QString& title,
                                         const QString& initialLabel,
                                         HierarchicalPortItem::PortType initialType,
                                         bool showType) {
    NetLabelDialogResult result;

    QDialog dlg(parent);
    dlg.setWindowTitle(title);
    auto* form = new QFormLayout(&dlg);

    auto* nameEdit = new QLineEdit(&dlg);
    nameEdit->setText(initialLabel);
    form->addRow("Net Name:", nameEdit);

    QComboBox* typeCombo = nullptr;
    if (showType) {
        typeCombo = new QComboBox(&dlg);
        typeCombo->addItem("None", "none");
        typeCombo->addItem("Input", "input");
        typeCombo->addItem("Output", "output");
        typeCombo->addItem("Bi-Directional", "bidir");

        switch (initialType) {
            case HierarchicalPortItem::Input: typeCombo->setCurrentIndex(1); break;
            case HierarchicalPortItem::Output: typeCombo->setCurrentIndex(2); break;
            case HierarchicalPortItem::Bidirectional: typeCombo->setCurrentIndex(3); break;
            default: typeCombo->setCurrentIndex(0); break;
        }

        form->addRow("Pin Type:", typeCombo);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return result;

    const QString label = nameEdit->text().trimmed();
    if (label.isEmpty()) return result;

    result.accepted = true;
    result.label = label;
    if (typeCombo) {
        const QString t = typeCombo->currentData().toString();
        if (t == "input") result.portType = HierarchicalPortItem::Input;
        else if (t == "output") result.portType = HierarchicalPortItem::Output;
        else if (t == "bidir") result.portType = HierarchicalPortItem::Bidirectional;
        else result.portType = HierarchicalPortItem::Passive;
    } else {
        result.portType = initialType;
    }

    return result;
}

void SchematicNetLabelTool::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;

    QPointF scenePos = view()->mapToScene(event->pos());
    QPointF snappedPos = view()->snapToGridOrPin(scenePos).point;

    QString title = (m_scope == NetLabelItem::Global) ? "Global Label" : "Net Label";
    NetLabelDialogResult res = promptNetLabelDialog(view(), title, "NET", HierarchicalPortItem::Passive, m_scope == NetLabelItem::Local);
    if (!res.accepted) return;

    if (m_scope == NetLabelItem::Local) {
        // Place a visible pin-like port for local labels (LTspice-style pin marker).
        auto* item = new HierarchicalPortItem(snappedPos, res.label, res.portType);
        view()->undoStack()->push(new AddItemCommand(view()->scene(), item));
    } else {
        NetLabelItem* item = new NetLabelItem(snappedPos, res.label, nullptr, m_scope);
        view()->undoStack()->push(new AddItemCommand(view()->scene(), item));
    }
}
