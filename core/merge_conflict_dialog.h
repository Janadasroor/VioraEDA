#ifndef MERGE_CONFLICT_DIALOG_H
#define MERGE_CONFLICT_DIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QGroupBox>
#include <QJsonDocument>
#include "semantic_merge_engine.h"

/**
 * @brief Dialog for visually resolving merge conflicts.
 * 
 * Shows each conflict with:
 * - The JSON path of the conflict
 * - Base value (original)
 * - Our value (current working copy)
 * - Their value (incoming changes)
 * - Buttons to choose "Use Ours", "Use Theirs", or custom edit
 */
class MergeConflictDialog : public QDialog {
    Q_OBJECT

public:
    explicit MergeConflictDialog(MergeResult& result, QWidget* parent = nullptr);
    ~MergeConflictDialog();

    /**
     * @brief Get the resolved merge result after user interaction.
     */
    MergeResult resolvedResult() const { return m_result; }

private slots:
    void onConflictSelected(int row);
    void onUseOurs();
    void onUseTheirs();
    void onUseBase();
    void onResolveAllOurs();
    void onResolveAllTheirs();
    void onApply();

private:
    void setupUI();
    void populateConflicts();
    void updatePreview();

    MergeResult& m_result;

    QTableWidget* m_conflictTable;
    QTextEdit* m_basePreview;
    QTextEdit* m_oursPreview;
    QTextEdit* m_theirsPreview;
    QTextEdit* m_mergedPreview;

    QLabel* m_statusLabel;
    QPushButton* m_useOursBtn;
    QPushButton* m_useTheirsBtn;
    QPushButton* m_useBaseBtn;
    QPushButton* m_resolveAllOursBtn;
    QPushButton* m_resolveAllTheirsBtn;
    QPushButton* m_applyBtn;
    QPushButton* m_cancelBtn;

    int m_currentRow = -1;
};

#endif // MERGE_CONFLICT_DIALOG_H
