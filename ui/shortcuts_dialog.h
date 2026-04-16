#ifndef SHORTCUTS_DIALOG_H
#define SHORTCUTS_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QTableWidget>

class QLineEdit;
class QTableWidget;

class ShortcutsDialog : public QDialog {
    Q_OBJECT
public:
    explicit ShortcutsDialog(QWidget* parent = nullptr);

private Q_SLOTS:
    void onSearchChanged(const QString& text);

private:
    QLineEdit* m_searchBox;
    QTableWidget* m_table;
};

#endif // SHORTCUTS_DIALOG_H
