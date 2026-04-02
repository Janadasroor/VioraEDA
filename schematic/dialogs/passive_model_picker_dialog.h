#ifndef PASSIVE_MODEL_PICKER_DIALOG_H
#define PASSIVE_MODEL_PICKER_DIALOG_H

#include <QDialog>

class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QLabel;

class PassiveModelPickerDialog : public QDialog {
    Q_OBJECT

public:
    enum class Kind {
        Resistor,
        Capacitor,
        Inductor
    };

    explicit PassiveModelPickerDialog(Kind kind, QWidget* parent = nullptr);

    QString selectedModel() const;
    QString selectedValue() const;
    QString selectedManufacturer() const;
    QString selectedMpn() const;

private Q_SLOTS:
    void filterModels(const QString& text);
    void onModelSelected(QListWidgetItem* item);
    void applySelected();

private:
    void loadModels();

    Kind m_kind;
    QString m_selectedModel;
    QString m_selectedValue;
    QString m_selectedManufacturer;
    QString m_selectedMpn;
    QLineEdit* m_searchEdit = nullptr;
    QListWidget* m_modelList = nullptr;
    QLabel* m_detailLabel = nullptr;
};

#endif // PASSIVE_MODEL_PICKER_DIALOG_H
