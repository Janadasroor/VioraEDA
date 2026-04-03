#include "schematic_spice_directive_tool.h"
#include "../dialogs/spice_directive_dialog.h"
#include "../items/schematic_spice_directive_item.h"
#include "schematic_view.h"
#include "schematic_commands.h"
#include <QGraphicsSceneMouseEvent>
#include <QUndoStack>

SchematicSpiceDirectiveTool::SchematicSpiceDirectiveTool(QObject* parent) 
    : SchematicTool("Spice Directive", parent)
{
}

void SchematicSpiceDirectiveTool::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        QPointF pos = view()->snapToGrid(view()->mapToScene(event->pos()));

        auto* item = new SchematicSpiceDirectiveItem(QString(), pos);
        SpiceDirectiveDialog dlg(item, nullptr, view()->scene(), view());
        dlg.setWindowTitle("Place SPICE Directive");

        if (dlg.exec() == QDialog::Accepted && !item->text().trimmed().isEmpty()) {
            if (view()->undoStack()) {
                view()->undoStack()->push(new AddItemCommand(view()->scene(), item));
            } else {
                view()->scene()->addItem(item);
            }
        } else {
            delete item;
        }
    }
}
