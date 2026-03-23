#ifndef BJT_MODEL_PICKER_DIALOG_H
#define BJT_MODEL_PICKER_DIALOG_H

#include <QDialog>

class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QLabel;

class BjtModelPickerDialog : public QDialog {
    Q_OBJECT

public:
    explicit BjtModelPickerDialog(bool pnp, QWidget* parent = nullptr);

    QString selectedModel() const;

private slots:
    void filterModels(const QString& text);
    void onModelSelected(QListWidgetItem* item);
    void applySelected();

private:
    void loadModels();

    bool m_pnp = false;
    QString m_selectedModel;

    QLineEdit* m_searchEdit = nullptr;
    QListWidget* m_modelList = nullptr;
    QLabel* m_detailLabel = nullptr;
};

#endif // BJT_MODEL_PICKER_DIALOG_H
