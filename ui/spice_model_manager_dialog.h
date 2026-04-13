#ifndef SPICE_MODEL_MANAGER_DIALOG_H
#define SPICE_MODEL_MANAGER_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QTextBrowser>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTreeView>
#include <QSortFilterProxyModel>
#include <QTabWidget>
#include <QCheckBox>
#include <QSet>
#include <QStringList>
#include "spice_model_list_model.h"

class SearchHighlightDelegate;

class SpiceModelManagerDialog : public QDialog {
    Q_OBJECT
public:
    explicit SpiceModelManagerDialog(QWidget* parent = nullptr);

    // Allow external code to update used models
    void setUsedModels(const QSet<QString>& used);

private Q_SLOTS:
    void onSearchChanged(const QString& query);
    void onModelSelected(const QModelIndex& index);
    void onModelDoubleClicked(const QModelIndex& index);
    void onReloadLibraries();
    void onAddLibraryPath();
    void onToggleFavorite();
    void onApplyToSelectedComponent();
    void onExportCSV();
    void onShowStatistics();
    void onAdvancedFilterChanged();

private:
    void setupUI();
    void loadFavorites();
    void saveFavorites();
    void updateDetailsPanel(const QModelIndex& index);
    void updateStatsLabel();
    QString generateRawSpiceLine(const SpiceModelInfo& info) const;
    QString generatePinDiagram(const SpiceModelInfo& info) const;
    QSet<QString> getUsedModelsFromSchematic() const;

    QLineEdit* m_searchField;
    QTreeView* m_modelView;
    SpiceModelListModel* m_model;
    QSortFilterProxyModel* m_proxyModel;

    // Details panel
    QTabWidget* m_detailsTabs;
    QLabel* m_modelTitle;
    QLabel* m_modelMeta;
    QTextBrowser* m_modelDetails;
    QPlainTextEdit* m_rawSpicePreview;
    QTextBrowser* m_pinDiagramView;

    // Toolbar buttons
    QPushButton* m_reloadBtn;
    QPushButton* m_favBtn;
    QPushButton* m_applyBtn;
    QPushButton* m_exportBtn;
    QPushButton* m_statsBtn;

    // Advanced filter
    QWidget* m_advancedFilterPanel;
    QCheckBox* m_filterDuplicates;
    QCheckBox* m_filterFavorites;
    QCheckBox* m_filterUsed;
    QLineEdit* m_typeFilter;
    QLineEdit* m_paramFilter;

    // Stats label
    QLabel* m_statsLabel;

    // Persistence
    QSet<QString> m_favorites;
    QStringList m_recentModels;

    SearchHighlightDelegate* m_highlightDelegate;
};

#endif // SPICE_MODEL_MANAGER_DIALOG_H
