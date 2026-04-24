#ifndef POWER_STAGE_PROPERTIES_DIALOG_H
#define POWER_STAGE_PROPERTIES_DIALOG_H

#include "smart_properties_dialog.h"

class GenericComponentItem;

class PowerStagePropertiesDialog : public SmartPropertiesDialog {
    Q_OBJECT

public:
    PowerStagePropertiesDialog(GenericComponentItem* item, QUndoStack* undoStack = nullptr, QGraphicsScene* scene = nullptr, QWidget* parent = nullptr);

protected:
    void onApply() override;
    void applyPreview() override;

private Q_SLOTS:
    void onRedrawWaveform();

private:
    GenericComponentItem* m_genItem;
    QString m_subcktName;
};

#endif // POWER_STAGE_PROPERTIES_DIALOG_H
