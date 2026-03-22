#ifndef DIODE_MODEL_PICKER_DIALOG_H
#define DIODE_MODEL_PICKER_DIALOG_H

#include <QDialog>
#include <QPointer>

class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QLabel;
class SchematicItem;

class DiodeModelPickerDialog : public QDialog {
    Q_OBJECT

public:
    DiodeModelPickerDialog(SchematicItem* item, const QString& diodeType, QWidget* parent = nullptr);

    QString selectedModel() const;

private slots:
    void filterModels(const QString& text);
    void onModelSelected(QListWidgetItem* item);
    void applySelected();

private:
    void loadModels();

    QPointer<SchematicItem> m_item;
    QString m_diodeType;  // "silicon", "schottky", "zener", "led", etc.
    QString m_selectedModel;

    QLineEdit* m_searchEdit = nullptr;
    QListWidget* m_modelList = nullptr;
    QLabel* m_detailLabel = nullptr;
};

#endif // DIODE_MODEL_PICKER_DIALOG_H
