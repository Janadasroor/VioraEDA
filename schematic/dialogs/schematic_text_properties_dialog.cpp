#include "schematic_text_properties_dialog.h"
#include "../editor/schematic_commands.h"
#include "../../simulator/bridge/model_library_manager.h"
#include "../items/generic_component_item.h"
#include <QCompleter>

namespace {
bool itemValueRepresentsSpiceModel(const SchematicItem* item) {
    if (!item) return false;

    const QString prefix = item->referencePrefix().trimmed().toUpper();
    const QString typeName = item->itemTypeName().trimmed().toLower();

    return prefix == "D" || prefix == "Q" || prefix == "QN" || prefix == "QP" ||
           prefix == "J" || prefix == "JN" || prefix == "JP" ||
           prefix == "M" || prefix == "MN" || prefix == "MP" ||
           prefix == "Z" ||
           typeName.contains("diode") ||
           typeName.contains("transistor") ||
           typeName.contains("mos") ||
           typeName.contains("jfet") ||
           typeName.contains("mesfet") ||
           typeName.contains("bjt");
}
}

SchematicTextPropertiesDialog::SchematicTextPropertiesDialog(SchematicTextItem* item, QUndoStack* undoStack, QGraphicsScene* scene, QWidget* parent)
    : SmartPropertiesDialog({item}, undoStack, scene, parent), m_item(item) {
    m_initializing = true;
    setWindowTitle("Text Properties");

    // Detect if this label belongs to a component/owner item
    m_parentComponent = dynamic_cast<GenericComponentItem*>(item->parentItem());
    m_parentOwner = dynamic_cast<SchematicItem*>(item->parentItem());
    if (m_parentOwner) {
        m_originalOwnerState = m_parentOwner->toJson();
    }

    PropertyTab textTab;
    textTab.title = "Text";

    PropertyField text;
    text.name = "text";
    text.label = m_parentComponent ? "Label Text" : "Text Content";
    text.type = PropertyField::Text;
    textTab.fields.append(text);

    PropertyField fontSize;
    fontSize.name = "fontSize";
    fontSize.label = "Font Size";
    fontSize.type = PropertyField::Integer;
    fontSize.unit = "pt";
    textTab.fields.append(fontSize);

    addTab(textTab);

    PropertyTab styleTab;
    styleTab.title = "Style";

    PropertyField alignment;
    alignment.name = "alignment";
    alignment.label = "Alignment";
    alignment.type = PropertyField::Choice;
    alignment.choices = {"Left", "Center", "Right"};
    styleTab.fields.append(alignment);

    PropertyField rotation;
    rotation.name = "rotation";
    rotation.label = "Rotation";
    rotation.type = PropertyField::Double;
    rotation.unit = "°";
    styleTab.fields.append(rotation);

    addTab(styleTab);

    // Model autocomplete tab
    PropertyTab modelTab;
    modelTab.title = "Model";

    PropertyField modelName;
    modelName.name = "model";
    modelName.label = m_parentComponent ? "Component Model Name" : "SPICE Model Name";
    modelName.type = PropertyField::Text;
    modelTab.fields.append(modelName);

    addTab(modelTab);

    // Initialize values
    if (editsReferenceLabel() && m_parentOwner) {
        m_originalText = m_parentOwner->reference();
    } else if (editsValueLabel() && m_parentOwner) {
        m_originalText = m_parentOwner->value();
    } else {
        m_originalText = item->text();
    }
    setPropertyValue("text", m_originalText);

    m_originalFontSize = effectiveFontSize();
    setPropertyValue("fontSize", m_originalFontSize);

    m_originalRotation = item->rotation();
    setPropertyValue("rotation", m_originalRotation);

    m_originalAlignment = currentAlignmentText();
    setPropertyValue("alignment", m_originalAlignment);

    // For component labels, show the parent component's model
    if (m_parentOwner) {
        QString model = m_parentOwner->spiceModel().trimmed();
        if (model.isEmpty() && editsValueLabel() && ownerValueRepresentsSpiceModel()) {
            model = m_parentOwner->value().trimmed();
        }
        m_originalModel = model;
        m_originalOwnerReference = m_parentOwner->reference();
        m_originalOwnerValue = m_parentOwner->value();
        setPropertyValue("model", m_originalModel);
    } else if (m_parentComponent) {
        m_originalModel = m_parentComponent->spiceModel();
        setPropertyValue("model", m_originalModel);
    } else {
        m_originalModel = item->spiceModel();
        setPropertyValue("model", m_originalModel);
    }

    // Add autocomplete completer to model field
    if (auto* modelEdit = qobject_cast<QLineEdit*>(m_widgets.value("model"))) {
        modelEdit->setPlaceholderText("e.g. 2N7000, 1N4148, 2N2222");

        QStringList allModels;
        for (const auto& info : ModelLibraryManager::instance().allModels()) {
            allModels.append(info.name);
        }
        allModels.sort(Qt::CaseInsensitive);
        allModels.removeDuplicates();
        auto* completer = new QCompleter(allModels, this);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setFilterMode(Qt::MatchContains);
        completer->setCompletionMode(QCompleter::PopupCompletion);
        modelEdit->setCompleter(completer);
    }

    // When the visible value label is the model for the owning device, avoid
    // exposing a second editable model field that can drift from the text.
    if (editsValueLabel() && ownerValueRepresentsSpiceModel()) {
        setTabVisible(m_tabWidget->indexOf(m_widgets.value("model")->parentWidget()), false);
    }

    m_initializing = false;
}

void SchematicTextPropertiesDialog::onApply() {
    if (!validateAll()) return;
    if (!m_undoStack || !m_scene) return;

    m_undoStack->beginMacro("Update Text Properties");

    QString newText = getPropertyValue("text").toString();
    if (editsReferenceLabel() && m_parentOwner) {
        if (newText != m_originalOwnerReference) {
            m_undoStack->push(new ChangePropertyCommand(m_scene, m_parentOwner, "reference", m_originalOwnerReference, newText));
        }
    } else if (editsValueLabel() && m_parentOwner) {
        if (newText != m_originalOwnerValue) {
            m_undoStack->push(new ChangePropertyCommand(m_scene, m_parentOwner, "value", m_originalOwnerValue, newText));
        }
    } else if (newText != m_originalText) {
        m_undoStack->push(new ChangePropertyCommand(m_scene, m_item, "Text", m_originalText, newText));
    }

    int newSize = qMax(1, getPropertyValue("fontSize").toInt());
    if (newSize != m_originalFontSize) {
        m_undoStack->push(new ChangePropertyCommand(m_scene, m_item, "Font Size", m_originalFontSize, newSize));
    }

    double newRot = getPropertyValue("rotation").toDouble();
    if (std::abs(newRot - m_originalRotation) > 0.001) {
        m_undoStack->push(new RotateItemCommand(m_scene, {m_item}, newRot - m_originalRotation));
    }

    QString newAlign = getPropertyValue("alignment").toString();
    if (newAlign != m_originalAlignment) {
        m_undoStack->push(new ChangePropertyCommand(m_scene, m_item, "Alignment", m_originalAlignment, newAlign));
    }

    // Apply model to parent component if this is a component label
    QString newModel = getPropertyValue("model").toString().trimmed();
    if (editsValueLabel() && ownerValueRepresentsSpiceModel()) {
        newModel = newText.trimmed();
    }
    if (m_parentOwner) {
        if (newModel != m_originalModel) {
            m_undoStack->push(new ChangePropertyCommand(m_scene, m_parentOwner, "spiceModel", m_originalModel, newModel));
        }
    } else if (m_parentComponent) {
        if (newModel != m_originalModel) {
            m_undoStack->push(new ChangePropertyCommand(m_scene, m_parentComponent, "spiceModel", m_originalModel, newModel));
        }
    } else if (newModel != m_originalModel) {
        m_undoStack->push(new ChangePropertyCommand(m_scene, m_item, "spiceModel", m_originalModel, newModel));
    }

    m_undoStack->endMacro();
    refreshBaselineFromCurrent();
}

void SchematicTextPropertiesDialog::applyPreview() {
    if (m_initializing) return;

    QString newText = getPropertyValue("text").toString();
    int newSize = qMax(1, getPropertyValue("fontSize").toInt());
    double newRot = getPropertyValue("rotation").toDouble();
    QString newAlign = getPropertyValue("alignment").toString();
    QString newModel = getPropertyValue("model").toString().trimmed();
    if (editsValueLabel() && ownerValueRepresentsSpiceModel()) {
        newModel = newText.trimmed();
    }

    if (editsReferenceLabel() && m_parentOwner) {
        m_parentOwner->setReference(newText);
    } else if (editsValueLabel() && m_parentOwner) {
        m_parentOwner->setValue(newText);
    } else {
        m_item->setText(newText);
    }
    QFont f = m_item->font();
    f.setPointSize(newSize);
    m_item->setFont(f);
    m_item->setRotation(newRot);

    Qt::Alignment align = Qt::AlignLeft;
    if (newAlign == "Center") align = Qt::AlignCenter;
    else if (newAlign == "Right") align = Qt::AlignRight;
    m_item->setAlignment(align);

    // Apply model to parent component if this is a component label
    if (m_parentOwner) {
        m_parentOwner->setSpiceModel(newModel);
    } else if (m_parentComponent) {
        m_parentComponent->setSpiceModel(newModel);
    } else {
        m_item->setSpiceModel(newModel);
    }

    m_item->update();
}

void SchematicTextPropertiesDialog::reject() {
    if (m_parentOwner && !m_originalOwnerState.isEmpty()) {
        m_parentOwner->fromJson(m_originalOwnerState);
        m_parentOwner->update();
    }
    SmartPropertiesDialog::reject();
}

bool SchematicTextPropertiesDialog::editsReferenceLabel() const {
    return m_item && m_item->isSubItem() && m_item->name().compare("RefLabel", Qt::CaseInsensitive) == 0;
}

bool SchematicTextPropertiesDialog::editsValueLabel() const {
    return m_item && m_item->isSubItem() && m_item->name().compare("ValueLabel", Qt::CaseInsensitive) == 0;
}

bool SchematicTextPropertiesDialog::ownerValueRepresentsSpiceModel() const {
    return itemValueRepresentsSpiceModel(m_parentOwner);
}

int SchematicTextPropertiesDialog::effectiveFontSize() const {
    if (!m_item) return 12;

    const QFont font = m_item->font();
    if (font.pointSize() > 0) return font.pointSize();

    const qreal pointSizeF = font.pointSizeF();
    if (pointSizeF > 0.0) return qMax(1, qRound(pointSizeF));

    return 12;
}

QString SchematicTextPropertiesDialog::currentAlignmentText() const {
    if (!m_item) return "Left";

    if (m_item->alignment() == Qt::AlignCenter) return "Center";
    if (m_item->alignment() == Qt::AlignRight) return "Right";
    return "Left";
}

void SchematicTextPropertiesDialog::refreshBaselineFromCurrent() {
    if (!m_item) return;

    if (editsReferenceLabel() && m_parentOwner) {
        m_originalText = m_parentOwner->reference();
    } else if (editsValueLabel() && m_parentOwner) {
        m_originalText = m_parentOwner->value();
    } else {
        m_originalText = m_item->text();
    }

    m_originalFontSize = effectiveFontSize();
    m_originalRotation = m_item->rotation();
    m_originalAlignment = currentAlignmentText();

    if (m_parentOwner) {
        m_originalOwnerReference = m_parentOwner->reference();
        m_originalOwnerValue = m_parentOwner->value();
        QString model = m_parentOwner->spiceModel().trimmed();
        if (model.isEmpty() && editsValueLabel() && ownerValueRepresentsSpiceModel()) {
            model = m_parentOwner->value().trimmed();
        }
        m_originalModel = model;
        m_originalOwnerState = m_parentOwner->toJson();
    } else if (m_parentComponent) {
        m_originalModel = m_parentComponent->spiceModel();
    } else {
        m_originalModel = m_item->spiceModel();
    }

    m_originalStates[m_item->id()] = m_item->toJson();
}
