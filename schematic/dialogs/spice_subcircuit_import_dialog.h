#ifndef SPICE_SUBCIRCUIT_IMPORT_DIALOG_H
#define SPICE_SUBCIRCUIT_IMPORT_DIALOG_H

#include <QDialog>
#include <QStringList>
#include <QProcess>

class QCheckBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QComboBox;
class QTableWidget;
class SpiceHighlighter;
class QPushButton;
class QProgressBar;

class SpiceSubcircuitImportDialog : public QDialog {
    Q_OBJECT

public:
    struct Result {
        struct PinMapping {
            QString subcktPin;
            QString symbolPinName;
            int symbolPinNumber = 0;
        };

        QString subcktName;
        QString fileName;
        QString absolutePath;
        QString relativeIncludePath;
        QStringList pins;
        QList<PinMapping> pinMappings;
        QString netlistText;
        bool insertIncludeDirective = true;
        bool openSymbolEditor = true;
        bool autoPlaceAfterSave = true;
    };

    explicit SpiceSubcircuitImportDialog(const QString& projectDir,
                                         const QString& currentFilePath,
                                         QWidget* parent = nullptr);

    Result result() const { return m_result; }
    void setPreloadedNetlist(const QString& netlist);
    void setSuggestedLibraryFileName(const QString& fileName);

private Q_SLOTS:
    void onAccepted();
    void updateFromText();
    void onSelectedSubcktChanged();
    void onAiGenerateClicked();
    void onAiProcessFinished(int exitCode);
    void onAiProcessReadyRead();

private:
    struct ParsedSubckt {
        QString name;
        QStringList pins;
    };

    void setupUi();
    QString baseDirectory() const;
    QString suggestedFileName(const QString& subcktName) const;
    QString targetAbsolutePath() const;
    void refreshPathPreview();
    QList<ParsedSubckt> parseSubckts(const QString& text, QStringList* errors, QStringList* warnings) const;
    void rebuildPinTable(const QStringList& pins);

    QString m_projectDir;
    QString m_currentFilePath;
    Result m_result;

    QPlainTextEdit* m_textEdit;
    QLineEdit* m_nameEdit;
    QLineEdit* m_fileNameEdit;
    QLabel* m_pathPreviewLabel;
    QLabel* m_statusLabel;
    QLabel* m_subcktPickerLabel;
    QComboBox* m_subcktPicker;
    QTableWidget* m_pinTable;
    QCheckBox* m_insertIncludeCheck;
    QCheckBox* m_openSymbolEditorCheck;
    QCheckBox* m_autoPlaceAfterSaveCheck;
    QDialogButtonBox* m_buttonBox;
    SpiceHighlighter* m_highlighter;
    
    QPushButton* m_aiGenerateBtn;
    QProgressBar* m_progressBar;
    QProcess* m_aiProcess = nullptr;
    QString m_aiResponseBuffer;
    QString m_preferredLibraryFileName;
    QList<ParsedSubckt> m_parsedSubckts;
};

#endif
