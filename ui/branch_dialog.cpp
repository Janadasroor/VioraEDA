#include "branch_dialog.h"
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
    emit switchRequested(branch);
    close();
}

void BranchDialog::onCreateClicked() {
    QString name = m_newBranchEdit->text().trimmed();
    if (name.isEmpty()) { QMessageBox::warning(this, "Create", "Enter a branch name."); return; }
    emit createRequested(name);
    m_newBranchEdit->clear();
}

void BranchDialog::onDeleteClicked() {
    QString branch = selectedBranch();
    if (branch.isEmpty()) { QMessageBox::warning(this, "Delete", "Select a branch."); return; }
    if (branch == m_currentBranch) { QMessageBox::warning(this, "Delete", "Cannot delete current branch."); return; }
    emit deleteRequested(branch);
}

void BranchDialog::onMergeClicked() {
    QString branch = selectedBranch();
    if (branch.isEmpty()) { QMessageBox::warning(this, "Merge", "Select a branch."); return; }
    if (branch == m_currentBranch) { QMessageBox::warning(this, "Merge", "Cannot merge branch into itself."); return; }
    emit mergeRequested(branch);
}

void BranchDialog::onAddRemoteClicked() {
    QString name = m_remoteNameEdit->text().trimmed();
    QString url = m_remoteUrlEdit->text().trimmed();
    if (name.isEmpty() || url.isEmpty()) { QMessageBox::warning(this, "Add Remote", "Enter name and URL."); return; }
    emit addRemoteRequested(name, url);
    m_remoteNameEdit->clear();
    m_remoteUrlEdit->clear();
}

void BranchDialog::applyTheme() {
    setStyleSheet(
        "QDialog { background-color: #1e1e1e; color: #d4d4d4; }"
        "QListWidget { background-color: #252526; color: #d4d4d4; border: 1px solid #3c3c3c; }"
        "QListWidget::item:selected { background-color: #094771; }"
        "QGroupBox { color: #cccccc; border: 1px solid #3c3c3c; border-radius: 4px; margin-top: 8px; padding-top: 16px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; }"
        "QPushButton { background-color: #0e639c; color: white; border: none; padding: 6px 12px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #1177bb; }"
        "QLineEdit { background-color: #3c3c3c; color: #d4d4d4; border: 1px solid #3c3c3c; padding: 4px 8px; border-radius: 4px; }"
        "QLineEdit:focus { border: 1px solid #0e639c; }"
        "QLabel { color: #cccccc; }"
    );
}
