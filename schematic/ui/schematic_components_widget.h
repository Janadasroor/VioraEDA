#ifndef SCHEMATICCOMPONENTSWIDGET_H
#define SCHEMATICCOMPONENTSWIDGET_H

#include <QWidget>
#include <QTreeView>
#include <QSortFilterProxyModel>
#include "../../ui/symbol_list_model.h"
#include "../../simulator/bridge/model_library_manager.h"

#include <QTabWidget>
#include <QLineEdit>
#include <QLabel>

class SchematicComponentsWidget : public QWidget {
    Q_OBJECT

public:
    explicit SchematicComponentsWidget(QWidget *parent = nullptr);
    ~SchematicComponentsWidget();

    void populate();
    void focusSearch();

signals:
    void toolSelected(const QString &toolName);
    void symbolCreated(const QString &symbolName);
    void symbolPlacementRequested(const class SymbolDefinition& symbol);
    void modelAssignmentRequested(const QString& modelName);

private slots:
    void onSearchTextChanged(const QString &text);
    void onItemClicked(const QModelIndex& index);
    void onCreateSymbol();
    void onOpenLibraryBrowser();
    void onApplyModelRequested(const SpiceModelInfo& info);

private:
    QTabWidget *m_tabs;
    QWidget *m_symbolTab;
    class ModelBrowserWidget *m_modelTab;

    QLineEdit *m_searchBox;
    QTreeView *m_componentList;
    SymbolListModel *m_symbolListModel;
    QSortFilterProxyModel *m_proxyModel;

    SymbolDefinition m_selectedSymbol;
};

#endif // SCHEMATICCOMPONENTSWIDGET_H
