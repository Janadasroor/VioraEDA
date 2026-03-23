#ifndef CURRENT_SOURCE_PROPERTIES_DIALOG_H
#define CURRENT_SOURCE_PROPERTIES_DIALOG_H

#include "smart_properties_dialog.h"
#include "../items/current_source_item.h"

class CurrentSourcePropertiesDialog : public SmartPropertiesDialog {
    Q_OBJECT

public:
    CurrentSourcePropertiesDialog(CurrentSourceItem* item, QUndoStack* undoStack = nullptr, QGraphicsScene* scene = nullptr, QWidget* parent = nullptr);

protected:
    void onApply() override;
    void applyPreview() override;
    void onFieldChanged() override;

private:
    void updateTabVisibility();
    CurrentSourceItem* m_item;
};

#endif // CURRENT_SOURCE_PROPERTIES_DIALOG_H
