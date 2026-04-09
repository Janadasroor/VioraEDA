#ifndef FOOTPRINT_BROWSER_DIALOG_H
#define FOOTPRINT_BROWSER_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QTreeWidget>
#include <QListWidget>
#include <QLabel>
#include <QSplitter>
#include "../ui/footprint_preview_view.h"
#include "../../footprints/models/footprint_definition.h"

using Flux::Model::FootprintDefinition;

class FootprintBrowserDialog : public QDialog {
    Q_OBJECT

public:
    explicit FootprintBrowserDialog(QWidget *parent = nullptr);
    ~FootprintBrowserDialog();

    FootprintDefinition selectedFootprint() const { return m_selectedFootprint; }

Q_SIGNALS:
    void footprintSelected(const FootprintDefinition& footprint);

private Q_SLOTS:
    void onSearch();
    void onCategorySelected(QTreeWidgetItem* item, int column);
    void onResultSelected(QListWidgetItem* item);
    void onInsertClicked();

private:
    void setupUI();
    void performSearch(const QString& query);
    void updatePreview();

    QLineEdit* m_searchBox;
    QTreeWidget* m_librariesTree;
    QListWidget* m_resultsList;
    
    // Preview panel
    QLabel* m_previewTitle;
    QLabel* m_previewDesc;
    QLabel* m_previewStats;
    FootprintPreviewView* m_previewView;

    FootprintDefinition m_selectedFootprint;
    QList<FootprintDefinition> m_searchResults;
};

#endif // FOOTPRINT_BROWSER_DIALOG_H
