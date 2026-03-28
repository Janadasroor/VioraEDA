#ifndef SUBCIRCUIT_PICKER_DIALOG_H
#define SUBCIRCUIT_PICKER_DIALOG_H

#include <QDialog>

class QLabel;
class QCheckBox;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPlainTextEdit;

class SubcircuitPickerDialog : public QDialog {
    Q_OBJECT

public:
    explicit SubcircuitPickerDialog(const QString& currentModel = QString(),
                                    const QStringList& symbolPins = {},
                                    QWidget* parent = nullptr);

    QString selectedModel() const;
    QStringList selectedSubcktPins() const;
    bool applyByOrderRequested() const;
    bool applySmartMapRequested() const;
    QStringList selectedSuggestedMapping() const;

private slots:
    void filterModels(const QString& text);
    void onModelSelected(QListWidgetItem* item);
    void applySelected();

private:
    void loadModels(const QString& currentModel);
    void updateComparisonPreview(const QString& modelName);
    QStringList buildSmartMapping(const QStringList& subcktPins) const;

    QString m_selectedModel;
    QStringList m_symbolPins;
    bool m_applyByOrder = false;
    bool m_applySmartMap = false;
    QCheckBox* m_applyByOrderCheck = nullptr;
    QCheckBox* m_applySmartMapCheck = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QListWidget* m_modelList = nullptr;
    QLabel* m_detailLabel = nullptr;
    QPlainTextEdit* m_pinPreview = nullptr;
};

#endif
