#ifndef CCCS_PROPERTIES_DIALOG_H
#define CCCS_PROPERTIES_DIALOG_H

#include <QDialog>
#include <QPointer>

class QLineEdit;
class SchematicItem;

class CCCSPropertiesDialog : public QDialog {
    Q_OBJECT

public:
    explicit CCCSPropertiesDialog(SchematicItem* item, QWidget* parent = nullptr);

    QString controlSource() const;
    QString gainValue() const;

private slots:
    void updateCommandPreview();
    void applyChanges();

private:
    void setupUI();
    void loadValues();

    QPointer<SchematicItem> m_item;
    QLineEdit* m_controlSourceEdit = nullptr;
    QLineEdit* m_gainEdit = nullptr;
    QLineEdit* m_commandPreview = nullptr;
};

#endif // CCCS_PROPERTIES_DIALOG_H
