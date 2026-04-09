#include "merge_conflict_dialog.h"
#include "theme_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QJsonDocument>
#include <QSyntaxHighlighter>

MergeConflictDialog::MergeConflictDialog(MergeResult& result, QWidget* parent)
    : QDialog(parent), m_result(result)
{
    setWindowTitle("Resolve Merge Conflicts");
    resize(900, 650);
    setupUI();
    populateConflicts();
}

MergeConflictDialog::~MergeConflictDialog() = default;

void MergeConflictDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Header
    QLabel* header = new QLabel(
        QString("<b>%1 conflicts</b> found during merge. Review each conflict and choose how to resolve it.")
            .arg(m_result.conflictCount));
    header->setWordWrap(true);
    header->setStyleSheet("font-size: 14px; padding: 8px;");
    mainLayout->addWidget(header);

    // Conflict list
    m_conflictTable = new QTableWidget();
    m_conflictTable->setColumnCount(4);
    m_conflictTable->setHorizontalHeaderLabels({"Path", "Base", "Ours", "Theirs"});
    m_conflictTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_conflictTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_conflictTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_conflictTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_conflictTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_conflictTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mainLayout->addWidget(m_conflictTable);

    connect(m_conflictTable, &QTableWidget::currentCellChanged, this, &MergeConflictDialog::onConflictSelected);

    // Value previews
    QHBoxLayout* previewLayout = new QHBoxLayout();
    
    auto createPreviewGroup = [](const QString& title) -> QGroupBox* {
        QGroupBox* group = new QGroupBox(title);
        QVBoxLayout* layout = new QVBoxLayout(group);
        QTextEdit* edit = new QTextEdit();
        edit->setReadOnly(true);
        edit->setFont(QFont("Monospace", 10));
        layout->addWidget(edit);
        return group;
    };

    QGroupBox* baseGroup = createPreviewGroup("Base (Original)");
    m_basePreview = qobject_cast<QTextEdit*>(baseGroup->layout()->itemAt(0)->widget());
    previewLayout->addWidget(baseGroup);

    QGroupBox* oursGroup = createPreviewGroup("Ours (Current)");
    m_oursPreview = qobject_cast<QTextEdit*>(oursGroup->layout()->itemAt(0)->widget());
    previewLayout->addWidget(oursGroup);

    QGroupBox* theirsGroup = createPreviewGroup("Theirs (Incoming)");
    m_theirsPreview = qobject_cast<QTextEdit*>(theirsGroup->layout()->itemAt(0)->widget());
    previewLayout->addWidget(theirsGroup);

    mainLayout->addLayout(previewLayout);

    // Merged preview
    QGroupBox* mergedGroup = createPreviewGroup("Merged Result");
    m_mergedPreview = qobject_cast<QTextEdit*>(mergedGroup->layout()->itemAt(0)->widget());
    mainLayout->addWidget(mergedGroup);

    // Status
    m_statusLabel = new QLabel("Select a conflict to resolve");
    m_statusLabel->setStyleSheet("padding: 6px; font-weight: bold;");
    mainLayout->addWidget(m_statusLabel);

    // Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    
    m_useOursBtn = new QPushButton("Use Ours →");
    m_useTheirsBtn = new QPushButton("← Use Theirs");
    m_useBaseBtn = new QPushButton("Use Base");
    
    btnLayout->addWidget(m_useOursBtn);
    btnLayout->addWidget(m_useTheirsBtn);
    btnLayout->addWidget(m_useBaseBtn);
    btnLayout->addStretch();
    
    m_resolveAllOursBtn = new QPushButton("Resolve All: Use Ours");
    m_resolveAllTheirsBtn = new QPushButton("Resolve All: Use Theirs");
    btnLayout->addWidget(m_resolveAllOursBtn);
    btnLayout->addWidget(m_resolveAllTheirsBtn);

    m_cancelBtn = new QPushButton("Cancel");
    m_applyBtn = new QPushButton("Apply Resolved Merge");
    m_applyBtn->setStyleSheet("background-color: #28a745; color: white; font-weight: bold; padding: 8px;");
    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addWidget(m_applyBtn);
    mainLayout->addLayout(btnLayout);

    // Connections
    connect(m_useOursBtn, &QPushButton::clicked, this, &MergeConflictDialog::onUseOurs);
    connect(m_useTheirsBtn, &QPushButton::clicked, this, &MergeConflictDialog::onUseTheirs);
    connect(m_useBaseBtn, &QPushButton::clicked, this, &MergeConflictDialog::onUseBase);
    connect(m_resolveAllOursBtn, &QPushButton::clicked, this, &MergeConflictDialog::onResolveAllOurs);
    connect(m_resolveAllTheirsBtn, &QPushButton::clicked, this, &MergeConflictDialog::onResolveAllTheirs);
    connect(m_applyBtn, &QPushButton::clicked, this, &MergeConflictDialog::onApply);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void MergeConflictDialog::populateConflicts() {
    int conflictRow = 0;
    m_conflictTable->setRowCount(m_result.conflictCount);
    
    for (int i = 0; i < m_result.changes.size(); ++i) {
        const auto& change = m_result.changes[i];
        if (change.type != ChangeType::Conflict) continue;
        
        m_conflictTable->setItem(conflictRow, 0, new QTableWidgetItem(change.path));
        
        auto formatValue = [](const QVariant& val) -> QString {
            if (!val.isValid()) return "<empty>";
            if (val.typeId() == QMetaType::QString) return val.toString().left(50);
            if (val.typeId() == QMetaType::Int) return QString::number(val.toInt());
            if (val.typeId() == QMetaType::Double) return QString::number(val.toDouble(), 'f', 3);
            if (val.typeId() == QMetaType::Bool) return val.toBool() ? "true" : "false";
            return val.toString().left(50);
        };
        
        m_conflictTable->setItem(conflictRow, 1, new QTableWidgetItem(formatValue(change.baseValue)));
        m_conflictTable->setItem(conflictRow, 2, new QTableWidgetItem(formatValue(change.oursValue)));
        m_conflictTable->setItem(conflictRow, 3, new QTableWidgetItem(formatValue(change.theirsValue)));
        
        // Store index for lookup
        m_conflictTable->item(conflictRow, 0)->setData(Qt::UserRole, i);
        
        conflictRow++;
    }
    
    if (m_conflictTable->rowCount() > 0) {
        m_conflictTable->selectRow(0);
    }
}

void MergeConflictDialog::onConflictSelected(int row) {
    if (row < 0 || row >= m_conflictTable->rowCount()) return;
    
    m_currentRow = row;
    int changeIdx = m_conflictTable->item(row, 0)->data(Qt::UserRole).toInt();
    const auto& change = m_result.changes[changeIdx];
    
    auto toJson = [](const QVariant& val) -> QString {
        if (!val.isValid()) return "null";
        QJsonObject obj;
        if (val.typeId() == QMetaType::QString) {
            obj["value"] = val.toString();
        } else if (val.typeId() == QMetaType::Int) {
            obj["value"] = val.toInt();
        } else if (val.typeId() == QMetaType::Double) {
            obj["value"] = val.toDouble();
        } else if (val.typeId() == QMetaType::Bool) {
            obj["value"] = val.toBool();
        } else {
            obj["value"] = val.toString();
        }
        return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    };
    
    m_basePreview->setPlainText(toJson(change.baseValue));
    m_oursPreview->setPlainText(toJson(change.oursValue));
    m_theirsPreview->setPlainText(toJson(change.theirsValue));
    
    updatePreview();
    
    m_statusLabel->setText(QString("Conflict %1/%2: %3")
        .arg(row + 1).arg(m_conflictTable->rowCount()).arg(change.path));
}

void MergeConflictDialog::updatePreview() {
    if (m_currentRow < 0 || m_currentRow >= m_conflictTable->rowCount()) return;
    
    int changeIdx = m_conflictTable->item(m_currentRow, 0)->data(Qt::UserRole).toInt();
    auto& change = m_result.changes[changeIdx];
    
    if (!change.resolved) return;
    
    QJsonObject obj;
    obj["resolved"] = true;
    obj["value"] = change.mergedValue.toString();
    
    m_mergedPreview->setPlainText(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Indented)));
}

void MergeConflictDialog::onUseOurs() {
    if (m_currentRow < 0) return;
    int changeIdx = m_conflictTable->item(m_currentRow, 0)->data(Qt::UserRole).toInt();
    SemanticMergeEngine::resolveConflict(m_result.changes[changeIdx], true);
    m_conflictTable->item(m_currentRow, 2)->setForeground(Qt::green);
    m_statusLabel->setText("Resolved: Using Ours value");
    m_statusLabel->setStyleSheet("padding: 6px; font-weight: bold; color: #28a745;");
    updatePreview();
}

void MergeConflictDialog::onUseTheirs() {
    if (m_currentRow < 0) return;
    int changeIdx = m_conflictTable->item(m_currentRow, 0)->data(Qt::UserRole).toInt();
    SemanticMergeEngine::resolveConflict(m_result.changes[changeIdx], false);
    m_conflictTable->item(m_currentRow, 3)->setForeground(Qt::green);
    m_statusLabel->setText("Resolved: Using Theirs value");
    m_statusLabel->setStyleSheet("padding: 6px; font-weight: bold; color: #17a2b8;");
    updatePreview();
}

void MergeConflictDialog::onUseBase() {
    if (m_currentRow < 0) return;
    int changeIdx = m_conflictTable->item(m_currentRow, 0)->data(Qt::UserRole).toInt();
    auto& change = m_result.changes[changeIdx];
    change.mergedValue = change.baseValue;
    change.resolved = true;
    change.type = ChangeType::Modified;
    m_conflictTable->item(m_currentRow, 1)->setForeground(Qt::yellow);
    m_statusLabel->setText("Resolved: Using Base (original) value");
    m_statusLabel->setStyleSheet("padding: 6px; font-weight: bold; color: #ffc107;");
    updatePreview();
}

void MergeConflictDialog::onResolveAllOurs() {
    for (auto& change : m_result.changes) {
        if (change.type == ChangeType::Conflict && !change.resolved) {
            SemanticMergeEngine::resolveConflict(change, true);
        }
    }
    m_statusLabel->setText("All conflicts resolved: Using Ours values");
    m_statusLabel->setStyleSheet("padding: 6px; font-weight: bold; color: #28a745;");
    QMessageBox::information(this, "Resolved", "All conflicts resolved using 'Ours' values.");
}

void MergeConflictDialog::onResolveAllTheirs() {
    for (auto& change : m_result.changes) {
        if (change.type == ChangeType::Conflict && !change.resolved) {
            SemanticMergeEngine::resolveConflict(change, false);
        }
    }
    m_statusLabel->setText("All conflicts resolved: Using Theirs values");
    m_statusLabel->setStyleSheet("padding: 6px; font-weight: bold; color: #17a2b8;");
    QMessageBox::information(this, "Resolved", "All conflicts resolved using 'Theirs' values.");
}

void MergeConflictDialog::onApply() {
    // Check if all conflicts are resolved
    int unresolved = 0;
    for (const auto& change : m_result.changes) {
        if (change.type == ChangeType::Conflict && !change.resolved) {
            unresolved++;
        }
    }
    
    if (unresolved > 0) {
        QMessageBox::warning(this, "Unresolved Conflicts",
            QString("%1 conflicts remain unresolved. Please resolve all conflicts before applying.")
                .arg(unresolved));
        return;
    }
    
    m_result.success = true;
    m_result.hasConflicts = false;
    accept();
}
