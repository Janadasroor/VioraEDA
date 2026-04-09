#include "fanout_engine.h"
#include "../items/pad_item.h"
#include "../items/trace_item.h"
#include "../items/via_item.h"
#include "../editor/pcb_commands.h"
#include <cmath>
#include <QDebug>

FanoutEngine::Result FanoutEngine::generateFanout(QGraphicsScene* scene, ComponentItem* component, 
                                               const FanoutWizardDialog::FanoutOptions& options,
                                               QUndoStack* undoStack) {
    Result result;
    if (!scene || !component) {
        result.message = "Invalid scene or component";
        return result;
    }

    QList<PCBItem*> addedItems;
    const QPointF compPos = component->pos();
    const qreal compRot = component->rotation();
    
    // Iterate over pads
    QList<QGraphicsItem*> children = component->childItems();
    for (QGraphicsItem* child : children) {
        PadItem* pad = dynamic_cast<PadItem*>(child);
        if (!pad) continue;

        QString net = pad->netName();
        if (options.onlyPowerNets && !isPowerNet(net)) continue;
        if (!options.includeUnconnected && (net.isEmpty() || net == "No Net")) continue;

        const QPointF padScenePos = pad->scenePos();
        const QPointF padLocalPos = component->mapFromScene(padScenePos);
        
        // Calculate escape direction
        QPointF dir = padLocalPos;
        qreal len = std::hypot(dir.x(), dir.y());
        if (len < 1e-6) dir = QPointF(1, 0); else dir /= len;

        if (options.direction == FanoutWizardDialog::FanoutOptions::Inward) {
            dir = -dir;
        } else if (options.direction == FanoutWizardDialog::FanoutOptions::Alternating) {
            static int alt = 0;
            if (alt++ % 2 == 0) dir = -dir;
        }

        // Apply component rotation to direction
        qreal angleRad = qDegreesToRadians(compRot);
        qreal nx = dir.x() * std::cos(angleRad) - dir.y() * std::sin(angleRad);
        qreal ny = dir.x() * std::sin(angleRad) + dir.y() * std::cos(angleRad);
        QPointF escapeDir(nx, ny);

        QPointF viaPos = padScenePos + escapeDir * options.shortTraceLength;

        // Create Trace
        TraceItem* trace = new TraceItem(padScenePos, viaPos, options.traceWidth);
        trace->setLayer(component->layer());
        trace->setNetName(net);
        addedItems.append(trace);
        result.traceCount++;

        // Create Via
        ViaItem* via = new ViaItem(viaPos);
        via->setDiameter(options.viaDiameter);
        via->setDrillSize(options.viaDrill);
        via->setNetName(net);
        // Fan-out usually goes to an internal plane or opposite side
        via->setStartLayer(component->layer());
        via->setEndLayer(component->layer() == 0 ? 1 : 0); 
        addedItems.append(via);
        result.viaCount++;
    }

    if (!addedItems.isEmpty()) {
        if (undoStack) {
            undoStack->push(new PCBAddItemsCommand(scene, addedItems));
        } else {
            for (auto* item : addedItems) scene->addItem(item);
        }
        result.success = true;
        result.message = QString("Successfully generated %1 vias and %2 traces").arg(result.viaCount).arg(result.traceCount);
    } else {
        result.message = "No pads found to fan-out";
    }

    return result;
}

bool FanoutEngine::isPowerNet(const QString& net) {
    if (net.isEmpty()) return false;
    QString n = net.toUpper();
    return n.contains("VCC") || n.contains("VDD") || n.contains("GND") || 
           n.contains("VSS") || n.contains("PWR") || n.contains("POWER") ||
           n.contains("+5V") || n.contains("+3.3V") || n.contains("+12V");
}
