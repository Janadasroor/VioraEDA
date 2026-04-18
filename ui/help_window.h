#ifndef HELPWINDOW_H
#define HELPWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
#include <QTextBrowser>
#include <QSplitter>
#include <QLineEdit>

class HelpWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit HelpWindow(QWidget *parent = nullptr);
    ~HelpWindow();

    void loadGuide(const QString& filename);

private Q_SLOTS:
    void onDocSelected(QTreeWidgetItem* item, int column);
    void onSearchChanged(const QString& text);

private:
    void setupUi();
    void populateGuides();
    void applyTheme();
    QString markdownStyleSheet() const;

    QLineEdit* m_searchEdit;
    QTreeWidget* m_docTree;
    QTextBrowser* m_contentViewer;
    QSplitter* m_splitter;
};

#endif // HELPWINDOW_H
