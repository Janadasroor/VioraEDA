#ifndef PROJECT_EXPLORER_WIDGET_H
#define PROJECT_EXPLORER_WIDGET_H

#include <QWidget>
#include <QTreeView>
#include <QFileSystemModel>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QStack>

struct DeletedEntry {
    QString originalPath;
    QString trashPath;
    bool isDir;
};

class ProjectExplorerWidget : public QWidget {
    Q_OBJECT

public:
    explicit ProjectExplorerWidget(QWidget *parent = nullptr);
    void setRootPath(const QString& path);

signals:
    void fileDoubleClicked(const QString& filePath);

private slots:
    void onDoubleClicked(const QModelIndex& index);
    void onFilterChanged(const QString& text);
    void onContextMenuRequested(const QPoint& pos);
    void onRefreshRequested();
    void onCollapseAllRequested();

private:
    void setupUi();
    void applyTheme();
    void deleteItem(const QString& path, bool isDir);
    void undoLastDelete();
    QString trashDir() const;

    QTreeView* m_treeView;
    QFileSystemModel* m_model;
    class FileFilterProxyModel* m_proxyModel;
    QLineEdit* m_searchBox;
    QLabel* m_titleLabel;
    QToolButton* m_undoBtn;
    QString m_rootPath;
    QStack<DeletedEntry> m_deleteHistory;
};

#endif // PROJECT_EXPLORER_WIDGET_H
