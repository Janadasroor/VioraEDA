#ifndef SCHEMATIC_ITEM_SELECTION_UTILS_H
#define SCHEMATIC_ITEM_SELECTION_UTILS_H

#include "schematic_item.h"
#include <QGraphicsScene>
#include <QSet>

inline QList<SchematicItem*> topLevelSchematicItemsFromGraphicsItems(const QList<QGraphicsItem*>& graphicsItems) {
    QList<SchematicItem*> result;
    QSet<SchematicItem*> seen;

    for (QGraphicsItem* graphicsItem : graphicsItems) {
        SchematicItem* schematicItem = nullptr;
        QGraphicsItem* current = graphicsItem;
        while (current) {
            schematicItem = dynamic_cast<SchematicItem*>(current);
            if (schematicItem && !schematicItem->isSubItem()) break;
            current = current->parentItem();
        }
        if (schematicItem && !seen.contains(schematicItem)) {
            seen.insert(schematicItem);
            result.append(schematicItem);
        }
    }

    return result;
}

inline QList<SchematicItem*> topLevelSelectedSchematicItems(const QGraphicsScene* scene) {
    return scene ? topLevelSchematicItemsFromGraphicsItems(scene->selectedItems()) : QList<SchematicItem*>();
}

#endif // SCHEMATIC_ITEM_SELECTION_UTILS_H
