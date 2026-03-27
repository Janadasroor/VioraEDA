#ifndef SPICE_MODEL_MANAGER_DIALOG_H
#define SPICE_MODEL_MANAGER_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QTextBrowser>
#include <QPushButton>
#include <QTreeView>
#include <QSortFilterProxyModel>
#include "spice_model_list_model.h"

class SpiceModelManagerDialog : public QDialog {
    Q_OBJECT
public:
    explicit SpiceModelManagerDialog(QWidget* parent = nullptr);

private slots:
    void onSearchChanged(const QString& query);
    void onModelSelected(const QModelIndex& index);
    void onReloadLibraries();
    void onAddLibraryPath();

private:
    void setupUI();
    
    QLineEdit* m_searchField;
    QTreeView* m_modelView;
    SpiceModelListModel* m_model;
    QSortFilterProxyModel* m_proxyModel;
    
    QLabel* m_modelTitle;
    QLabel* m_modelMeta;
    QTextBrowser* m_modelDetails;
    QPushButton* m_reloadBtn;
};

#endif // SPICE_MODEL_MANAGER_DIALOG_H
