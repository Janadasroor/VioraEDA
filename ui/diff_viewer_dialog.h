#ifndef DIFF_VIEWER_DIALOG_H
#define DIFF_VIEWER_DIALOG_H

#include <QDialog>
#include <QTextEdit>
#include <QLabel>

class DiffViewerDialog : public QDialog {
    Q_OBJECT

public:
    explicit DiffViewerDialog(QWidget* parent = nullptr);

    void setDiff(const QString& diff, const QString& filePath = QString());

private:
    void setupUi();
    void applyTheme();

    QTextEdit* m_diffView;
    QLabel* m_fileLabel;
};

#endif // DIFF_VIEWER_DIALOG_H
