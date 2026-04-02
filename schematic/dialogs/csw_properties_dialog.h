#ifndef CSW_PROPERTIES_DIALOG_H
#define CSW_PROPERTIES_DIALOG_H

#include <QDialog>
#include <QPointer>

class QLineEdit;
class SchematicItem;

class CSWPropertiesDialog : public QDialog {
    Q_OBJECT

public:
    explicit CSWPropertiesDialog(SchematicItem* item, QWidget* parent = nullptr);

    QString modelName() const;
    QString commandText() const;

private Q_SLOTS:
    void updateCommandPreview();
    void applyChanges();

private:
    void setupUI();
    void loadValues();

    QPointer<SchematicItem> m_item;

    QLineEdit* m_modelNameEdit = nullptr;
    QLineEdit* m_itEdit = nullptr;
    QLineEdit* m_ihEdit = nullptr;
    QLineEdit* m_ronEdit = nullptr;
    QLineEdit* m_roffEdit = nullptr;
    QLineEdit* m_ctrlSourceEdit = nullptr;
    QLineEdit* m_commandPreview = nullptr;
};

#endif // CSW_PROPERTIES_DIALOG_H
