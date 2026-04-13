#ifndef SCHEMATIC_TEXT_PROPERTIES_DIALOG_H
#define SCHEMATIC_TEXT_PROPERTIES_DIALOG_H

#include "smart_properties_dialog.h"
#include "../items/schematic_text_item.h"
#include <QPointer>

class GenericComponentItem;

class SchematicTextPropertiesDialog : public SmartPropertiesDialog {
    Q_OBJECT

public:
    SchematicTextPropertiesDialog(SchematicTextItem* item, QUndoStack* undoStack = nullptr, QGraphicsScene* scene = nullptr, QWidget* parent = nullptr);
    void reject() override;

protected:
    void onApply() override;
    void applyPreview() override;

private:
    void refreshBaselineFromCurrent();
    bool editsReferenceLabel() const;
    bool editsValueLabel() const;
    bool ownerValueRepresentsSpiceModel() const;
    int effectiveFontSize() const;
    QString currentAlignmentText() const;
    SchematicTextItem* m_item;
    QPointer<GenericComponentItem> m_parentComponent;
    QPointer<SchematicItem> m_parentOwner;
    QString m_originalText;
    int m_originalFontSize = 12;
    double m_originalRotation = 0.0;
    QString m_originalAlignment;
    QString m_originalOwnerReference;
    QString m_originalOwnerValue;
    QString m_originalModel;
    bool m_initializing = false;
    QJsonObject m_originalOwnerState;
};

#endif // SCHEMATIC_TEXT_PROPERTIES_DIALOG_H
