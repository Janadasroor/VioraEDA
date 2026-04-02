#include "branch_dialog.h"
#include "theme_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QInputDialog>

BranchDialog::BranchDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    applyTheme();
}

void BranchDialog::setBranches(const QVector<GitBranch>& branches, const QString& currentBranch) {
    m_branches = branches;
    m_currentBranch = currentBranch;
    refreshList();
}

void BranchDialog::setRemotes(const QStringList& remotes) {
    m_remotes = remotes;
}

void BranchDialog::setupUi() {
    setWindowTitle("Branches");
    resize(420, 500);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    // Branch list
    QLabel* listLabel = new QLabel("Branches:", this);
    listLabel->setStyleSheet("font-weight: bold;");
    layout->addWidget(listLabel);

    m_branchList = new QListWidget(this);
    layout->addWidget(m_branchList, 1);

    // Branch actions
    QHBoxLayout* branchBtns = new QHBoxLayout();
    QPushButton* switchBtn = new QPushButton("Switch", this);
    QPushButton* createBtn = new QPushButton("Create", this);
    QPushButton* deleteBtn = new QPushButton("Delete", this);
    QPushButton* mergeBtn = new QPushButton("Merge", this);
    branchBtns->addWidget(switchBtn);
    branchBtns->addWidget(createBtn);
    branchBtns->addWidget(deleteBtn);
    branchBtns->addWidget(mergeBtn);
    layout->addLayout(branchBtns);

    connect(switchBtn, &QPushButton::clicked, this, &BranchDialog::onSwitchClicked);
    connect(createBtn, &QPushButton::clicked, this, &BranchDialog::onCreateClicked);
    connect(deleteBtn, &QPushButton::clicked, this, &BranchDialog::onDeleteClicked);
    connect(mergeBtn, &QPushButton::clicked, this, &BranchDialog::onMergeClicked);

    // New branch
    QGroupBox* newBranchGroup = new QGroupBox("Create New Branch", this);
    QHBoxLayout* newBranchLayout = new QHBoxLayout(newBranchGroup);
    m_newBranchEdit = new QLineEdit(this);
    m_newBranchEdit->setText("main");
    m_newBranchEdit->setPlaceholderText("Branch name...");
    QPushButton* newBranchBtn = new QPushButton("Create & Switch", this);
    newBranchLayout->addWidget(m_newBranchEdit);
    newBranchLayout->addWidget(newBranchBtn);
    layout->addWidget(newBranchGroup);

    connect(newBranchBtn, &QPushButton::clicked, this, &BranchDialog::onCreateClicked);

    // Remote management
    QGroupBox* remoteGroup = new QGroupBox("Remotes", this);
    QVBoxLayout* remoteLayout = new QVBoxLayout(remoteGroup);
    QHBoxLayout* remoteInputs = new QHBoxLayout();
    m_remoteNameEdit = new QLineEdit(this);
    m_remoteNameEdit->setText("origin");
    m_remoteNameEdit->setPlaceholderText("Name (e.g. origin)");
    m_remoteUrlEdit = new QLineEdit(this);
    m_remoteUrlEdit->setPlaceholderText("URL (e.g. https://github.com/user/repo.git)");
    remoteInputs->addWidget(m_remoteNameEdit);
    remoteInputs->addWidget(m_remoteUrlEdit);
    remoteLayout->addLayout(remoteInputs);
    QPushButton* addRemoteBtn = new QPushButton("Add Remote", this);
    remoteLayout->addWidget(addRemoteBtn);
    layout->addWidget(remoteGroup);

    connect(addRemoteBtn, &QPushButton::clicked, this, &BranchDialog::onAddRemoteClicked);

    // Close
    QPushButton* closeBtn = new QPushButton("Close", this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
    layout->addWidget(closeBtn, 0, Qt::AlignRight);
}

void BranchDialog::refreshList() {
    m_branchList->clear();
    for (const auto& b : m_branches) {
        QString display = b.name;
        if (b.isCurrent) display = "● " + display;
        else display = "  " + display;

        if (b.isRemote) display += "  [remote]";

        QListWidgetItem* item = new QListWidgetItem(display);
        if (b.isCurrent) {
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
        }
        item->setData(Qt::UserRole, b.name);
        m_branchList->addItem(item);
    }
}

QString BranchDialog::selectedBranch() const {
    auto* item = m_branchList->currentItem();
    if (!item) return QString();
    return item->data(Qt::UserRole).toString();
}

void BranchDialog::onSwitchClicked() {
    QString branch = selectedBranch();
    if (branch.isEmpty()) { QMessageBox::warning(this, "Switch", "Select a branch."); return; }
    Q_EMIT switchRequested(branch);
    close();
}

void BranchDialog::onCreateClicked() {
    QString name = m_newBranchEdit->text().trimmed();
    if (name.isEmpty()) { QMessageBox::warning(this, "Create", "Enter a branch name."); return; }
    Q_EMIT createRequested(name);
    m_newBranchEdit->clear();
}

void BranchDialog::onDeleteClicked() {
    QString branch = selectedBranch();
    if (branch.isEmpty()) { QMessageBox::warning(this, "Delete", "Select a branch."); return; }
    if (branch == m_currentBranch) { QMessageBox::warning(this, "Delete", "Cannot delete current branch."); return; }
    Q_EMIT deleteRequested(branch);
}

void BranchDialog::onMergeClicked() {
    QString branch = selectedBranch();
    if (branch.isEmpty()) { QMessageBox::warning(this, "Merge", "Select a branch."); return; }
    if (branch == m_currentBranch) { QMessageBox::warning(this, "Merge", "Cannot merge branch into itself."); return; }
    Q_EMIT mergeRequested(branch);
}

void BranchDialog::onAddRemoteClicked() {
    QString name = m_remoteNameEdit->text().trimmed();
    QString url = m_remoteUrlEdit->text().trimmed();
    if (name.isEmpty() || url.isEmpty()) { QMessageBox::warning(this, "Add Remote", "Enter name and URL."); return; }
    Q_EMIT addRemoteRequested(name, url);
    m_remoteNameEdit->clear();
    m_remoteUrlEdit->clear();
}

void BranchDialog::applyTheme() {
    bool isLight = ThemeManager::theme() && ThemeManager::theme()->type() == PCBTheme::Light;

    if (isLight) {
        setStyleSheet(
            "QDialog { background-color: #f8fafc; color: #1e293b; }"
            "QListWidget { background-color: #ffffff; color: #1e293b; border: 1px solid #e2e8f0; border-radius: 6px; }"
            "QListWidget::item { padding: 4px 8px; border-radius: 4px; margin: 1px 4px; }"
            "QListWidget::item:selected { background-color: #dbeafe; color: #1e40af; }"
            "QGroupBox { color: #334155; border: 1px solid #cbd5e1; border-radius: 6px; margin-top: 8px; padding-top: 16px; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 8px; }"
            "QPushButton { background-color: #ffffff; color: #334155; border: 1px solid #cbd5e1; padding: 6px 12px; border-radius: 6px; font-weight: 500; }"
            "QPushButton:hover { background-color: #f1f5f9; border-color: #94a3b8; }"
            "QPushButton:pressed { background-color: #e2e8f0; border-color: #64748b; }"
            "QLineEdit { background-color: #ffffff; color: #1e293b; border: 1px solid #cbd5e1; padding: 6px 8px; border-radius: 6px; }"
            "QLineEdit:focus { border-color: #3b82f6; }"
            "QLabel { color: #334155; }"
        );
    } else {
        setStyleSheet(
            "QDialog { background-color: #1e1e1e; color: #d4d4d4; }"
            "QListWidget { background-color: #252526; color: #d4d4d4; border: 1px solid #3c3c3c; }"
            "QListWidget::item:selected { background-color: #094771; }"
            "QGroupBox { color: #cccccc; border: 1px solid #3c3c3c; border-radius: 4px; margin-top: 8px; padding-top: 16px; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 8px; }"
            "QPushButton { background-color: #0e639c; color: white; border: none; padding: 6px 12px; border-radius: 4px; }"
            "QPushButton:hover { background-color: #1177bb; }"
            "QLineEdit { background-color: #3c3c3c; color: #d4d4d4; border: 1px solid #3c3c3c; padding: 6px 8px; border-radius: 4px; }"
            "QLineEdit:focus { border-color: #0e639c; }"
            "QLabel { color: #cccccc; }"
        );
    }
}
