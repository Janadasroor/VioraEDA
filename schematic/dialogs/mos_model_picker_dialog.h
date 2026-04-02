#ifndef MOS_MODEL_PICKER_DIALOG_H
#define MOS_MODEL_PICKER_DIALOG_H

#include <QDialog>

class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QLabel;

class MosModelPickerDialog : public QDialog {
    Q_OBJECT

public:
    explicit MosModelPickerDialog(bool pmos, QWidget* parent = nullptr);

    QString selectedModel() const;

private Q_SLOTS:
    void filterModels(const QString& text);
    void onModelSelected(QListWidgetItem* item);
    void applySelected();

private:
    void loadModels();

    bool m_pmos = false;
    QString m_selectedModel;

    QLineEdit* m_searchEdit = nullptr;
    QListWidget* m_modelList = nullptr;
    QLabel* m_detailLabel = nullptr;
};

#endif // MOS_MODEL_PICKER_DIALOG_H
