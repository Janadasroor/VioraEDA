#ifndef FANOUT_ENGINE_H
#define FANOUT_ENGINE_H

#include <QGraphicsScene>
#include <QUndoStack>
#include "../items/component_item.h"
#include "../dialogs/fanout_wizard_dialog.h"

class FanoutEngine {
public:
    struct Result {
        bool success = false;
        int viaCount = 0;
        int traceCount = 0;
        QString message;
    };

    static Result generateFanout(QGraphicsScene* scene, ComponentItem* component, 
                               const FanoutWizardDialog::FanoutOptions& options,
                               QUndoStack* undoStack = nullptr);

private:
    static bool isPowerNet(const QString& net);
};

#endif // FANOUT_ENGINE_H
