#ifndef MODEL_BROWSER_WIDGET_H
#define MODEL_BROWSER_WIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QTreeView>
#include <QSortFilterProxyModel>
#include "../../ui/spice_model_list_model.h"

class ModelBrowserWidget : public QWidget {
    Q_OBJECT
public:
    explicit ModelBrowserWidget(QWidget* parent = nullptr);
    ~ModelBrowserWidget();

Q_SIGNALS:
    void modelSelected(const SpiceModelInfo& info);
    void applyModelRequested(const SpiceModelInfo& info);

private Q_SLOTS:
    void onSearchChanged(const QString& text);
    void onItemSelectionChanged(const QModelIndex& current);
    void onApplyClicked();
    void onReloadClicked();
    void onLibraryReloaded();

private:
    void setupUI();

    QLineEdit* m_searchBox;
    QTreeView* m_treeView;
    SpiceModelListModel* m_model;
    QSortFilterProxyModel* m_proxyModel;
    
    QLabel* m_detailLabel;
    QPushButton* m_applyBtn;
};

#endif // MODEL_BROWSER_WIDGET_H
