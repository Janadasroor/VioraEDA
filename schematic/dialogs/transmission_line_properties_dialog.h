#ifndef TRANSMISSION_LINE_PROPERTIES_DIALOG_H
#define TRANSMISSION_LINE_PROPERTIES_DIALOG_H

#include <QDialog>
#include <QMap>
#include <QPointer>

class QLineEdit;
class QGraphicsScene;
class QUndoStack;
class SchematicItem;

class TransmissionLinePropertiesDialog : public QDialog {
    Q_OBJECT

public:
    explicit TransmissionLinePropertiesDialog(SchematicItem* item, QGraphicsScene* scene = nullptr, QUndoStack* undoStack = nullptr, QWidget* parent = nullptr);

    QString valueString() const;
    QMap<QString, QString> ltraParams() const;

    bool wantsDirectiveUpdate() const { return m_wantsDirectiveUpdate; }
    QString directiveText() const { return m_directiveText; }
    QString originalModelName() const { return m_originalModelName; }

private Q_SLOTS:
    void updateCommandPreview();
    void applyChanges();
    void onInsertOrUpdateModelDirective();

private:
    void setupUI();
    void loadValues();

    QPointer<SchematicItem> m_item;
    QPointer<QGraphicsScene> m_scene;
    QPointer<QUndoStack> m_undoStack;
    bool m_isLossy = false;

    QLineEdit* m_z0Edit = nullptr;
    QLineEdit* m_tdEdit = nullptr;
    QLineEdit* m_modelEdit = nullptr;
    QLineEdit* m_rEdit = nullptr;
    QLineEdit* m_lEdit = nullptr;
    QLineEdit* m_gEdit = nullptr;
    QLineEdit* m_cEdit = nullptr;
    QLineEdit* m_lenEdit = nullptr;
    QLineEdit* m_commandPreview = nullptr;

    bool m_wantsDirectiveUpdate = false;
    QString m_directiveText;
    QString m_originalModelName;
};

#endif // TRANSMISSION_LINE_PROPERTIES_DIALOG_H
