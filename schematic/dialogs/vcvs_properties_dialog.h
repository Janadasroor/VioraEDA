#ifndef VCVS_PROPERTIES_DIALOG_H
#define VCVS_PROPERTIES_DIALOG_H

#include <QDialog>
#include <QPointer>

class QLineEdit;
class QLabel;
class SchematicItem;

class VCVSPropertiesDialog : public QDialog {
    Q_OBJECT

public:
    explicit VCVSPropertiesDialog(SchematicItem* item, QWidget* parent = nullptr);

    QString gainValue() const;

private slots:
    void updateCommandPreview();
    void applyChanges();

private:
    void setupUI();
    void loadValues();

    QPointer<SchematicItem> m_item;

    QLineEdit* m_gainEdit = nullptr;
    QLineEdit* m_commandPreview = nullptr;
};

#endif // VCVS_PROPERTIES_DIALOG_H
