#ifndef SCHEMATIC_NET_LABEL_TOOL_H
#define SCHEMATIC_NET_LABEL_TOOL_H

#include "schematic_tool.h"
#include "../items/net_label_item.h"
#include "../items/hierarchical_port_item.h"

struct NetLabelDialogResult {
    bool accepted = false;
    QString label;
    HierarchicalPortItem::PortType portType = HierarchicalPortItem::Passive;
};

NetLabelDialogResult promptNetLabelDialog(QWidget* parent,
                                         const QString& title,
                                         const QString& initialLabel = "NET",
                                         HierarchicalPortItem::PortType initialType = HierarchicalPortItem::Passive,
                                         bool showType = true);

class SchematicNetLabelTool : public SchematicTool {
    Q_OBJECT
public:
    SchematicNetLabelTool(NetLabelItem::LabelScope scope = NetLabelItem::Local, QObject* parent = nullptr);
    virtual ~SchematicNetLabelTool() = default;

    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    NetLabelItem::LabelScope m_scope;
};

#endif // SCHEMATIC_NET_LABEL_TOOL_H
