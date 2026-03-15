#include "project_explorer_widget.h"
#include "../core/theme_manager.h"
#include <QHeaderView>
#include <QSortFilterProxyModel>
#include <QMenu>
#include <QClipboard>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QInputDialog>
#include <QMessageBox>
#include <QToolButton>
#include <QAction>
#include <QRegularExpression>

class FileFilterProxyModel : public QSortFilterProxyModel {
public:
    explicit FileFilterProxyModel(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}
protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override {
        QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
        if (sourceModel()->hasChildren(index)) return true; // Always show directories
        
        QString fileName = sourceModel()->data(index).toString().toLower();
        // Only show relevant engineering files
        return fileName.endsWith(".sch") || fileName.endsWith(".sym") || 
               fileName.endsWith(".lib") || fileName.endsWith(".sclib") ||
               fileName.contains(filterRegularExpression());
    }
};

ProjectExplorerWidget::ProjectExplorerWidget(QWidget *parent)
    : QWidget(parent)
{
    m_model = new QFileSystemModel(this);
    m_model->setReadOnly(true);
    m_model->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    
    m_proxyModel = new FileFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);

    setupUi();
    applyTheme();
}

void ProjectExplorerWidget::setupUi() {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // --- Header Section (VS Code style) ---
    QWidget* header = new QWidget(this);
    header->setFixedHeight(32);
    header->setObjectName("ExplorerHeader");
    QHBoxLayout* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 0, 4, 0);
    headerLayout->setSpacing(4);

    m_titleLabel = new QLabel("PROJECT", header);
    m_titleLabel->setStyleSheet("font-weight: bold; font-size: 10px; color: #888;");
    headerLayout->addWidget(m_titleLabel, 1);

    auto addHeaderAction = [&](const QString& iconPath, const QString& tooltip, auto slot) {
        QToolButton* btn = new QToolButton(header);
        btn->setIcon(QIcon(iconPath));
        btn->setIconSize(QSize(14, 14));
        btn->setToolTip(tooltip);
        btn->setFixedSize(20, 20);
        btn->setAutoRaise(true);
        btn->setStyleSheet("QToolButton { background: transparent; border: none; padding: 2px; border-radius: 3px; } "
                          "QToolButton:hover { background-color: rgba(128, 128, 128, 0.15); }");
        connect(btn, &QToolButton::clicked, this, slot);
        headerLayout->addWidget(btn);
        return btn;
    };

    addHeaderAction(":/icons/tool_sync.svg", "Refresh", [this]() { this->onRefreshRequested(); });

    layout->addWidget(header);

    // Search bar container for better padding
    QWidget* searchContainer = new QWidget(this);
    QVBoxLayout* searchLayout = new QVBoxLayout(searchContainer);
    searchLayout->setContentsMargins(8, 4, 8, 8);
    searchLayout->setSpacing(0);

    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText("Filter files...");
    m_searchBox->setFixedHeight(28);
    m_searchBox->setClearButtonEnabled(true);
    
    // Add search icon to line edit
    QAction* searchAction = new QAction(QIcon(":/icons/tool_select.svg"), "", m_searchBox);
    m_searchBox->addAction(searchAction, QLineEdit::LeadingPosition);
    
    searchLayout->addWidget(m_searchBox);
    layout->addWidget(searchContainer);

    // Tree View
    m_treeView = new QTreeView(this);
    m_treeView->setModel(m_proxyModel);
    m_treeView->setHeaderHidden(true);
    m_treeView->setAnimated(true);
    m_treeView->setIndentation(14);
    m_treeView->setIconSize(QSize(16, 16));
    m_treeView->setSortingEnabled(true);
    m_treeView->sortByColumn(0, Qt::AscendingOrder);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeView->setUniformRowHeights(true);
    m_treeView->setAlternatingRowColors(true);
    
    // Hide size, type, date columns
    for (int i = 1; i < 4; ++i) m_treeView->hideColumn(i);
    
    layout->addWidget(m_treeView, 1);

    connect(m_treeView, &QTreeView::doubleClicked, this, &ProjectExplorerWidget::onDoubleClicked);
    connect(m_treeView, &QTreeView::customContextMenuRequested, this, &ProjectExplorerWidget::onContextMenuRequested);
    connect(m_searchBox, &QLineEdit::textChanged, this, &ProjectExplorerWidget::onFilterChanged);
}

void ProjectExplorerWidget::onRefreshRequested() {
    if (!m_rootPath.isEmpty()) {
        m_model->setRootPath("");
        m_model->setRootPath(m_rootPath);
    }
}

void ProjectExplorerWidget::onCollapseAllRequested() {
    if (m_treeView) {
        m_treeView->collapseAll();
    }
}

void ProjectExplorerWidget::setRootPath(const QString& path) {
    m_rootPath = path;
    m_model->setRootPath(path);
    m_treeView->setRootIndex(m_proxyModel->mapFromSource(m_model->index(path)));
}

void ProjectExplorerWidget::onDoubleClicked(const QModelIndex& index) {
    QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
    QString path = m_model->filePath(sourceIndex);
    if (QFileInfo(path).isFile()) {
        emit fileDoubleClicked(path);
    }
}

void ProjectExplorerWidget::onFilterChanged(const QString& text) {
    if (text.isEmpty()) {
        m_proxyModel->setFilterRegularExpression(QRegularExpression());
    } else {
        m_proxyModel->setFilterRegularExpression(QRegularExpression(text, QRegularExpression::CaseInsensitiveOption));
    }
}

void ProjectExplorerWidget::onContextMenuRequested(const QPoint& pos) {
    QModelIndex index = m_treeView->indexAt(pos);
    if (!index.isValid()) return;

    QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
    QString path = m_model->filePath(sourceIndex);
    if (path.isEmpty()) return;

    QFileInfo info(path);
    QMenu menu(this);

    if (info.isFile()) {
        QAction* openAct = menu.addAction("Open");
        connect(openAct, &QAction::triggered, this, [this, path]() { emit fileDoubleClicked(path); });
    } else {
        QAction* expandAct = menu.addAction(m_treeView->isExpanded(index) ? "Collapse" : "Expand");
        connect(expandAct, &QAction::triggered, this, [this, index]() {
            if (m_treeView->isExpanded(index)) m_treeView->collapse(index);
            else m_treeView->expand(index);
        });
    }

    menu.addSeparator();

    QAction* renameAct = menu.addAction("Rename...");
    connect(renameAct, &QAction::triggered, this, [this, path, info]() {
        const QString currentName = info.fileName();
        bool ok = false;
        QString newName = QInputDialog::getText(this, "Rename", "New name:", QLineEdit::Normal, currentName, &ok);
        if (!ok || newName.isEmpty() || newName == currentName) return;

        const QString newPath = info.absoluteDir().absoluteFilePath(newName);
        if (QFileInfo::exists(newPath)) {
            QMessageBox::warning(this, "Rename Failed", "A file or folder with that name already exists.");
            return;
        }
        bool success = info.isDir() ? QDir().rename(path, newPath) : QFile::rename(path, newPath);
        if (!success) {
            QMessageBox::warning(this, "Rename Failed", "Could not rename item.");
            return;
        }
        onRefreshRequested();
    });

    QAction* copyPathAct = menu.addAction("Copy Path");
    connect(copyPathAct, &QAction::triggered, this, [path]() {
        QApplication::clipboard()->setText(path);
    });

    if (!m_rootPath.isEmpty()) {
        const QString rel = QDir(m_rootPath).relativeFilePath(path);
        QAction* copyRelAct = menu.addAction("Copy Relative Path");
        connect(copyRelAct, &QAction::triggered, this, [rel]() {
            QApplication::clipboard()->setText(rel);
        });
    }

    QAction* revealAct = menu.addAction("Reveal in File Manager");
    connect(revealAct, &QAction::triggered, this, [path, info]() {
        const QString dir = info.isDir() ? path : info.absolutePath();
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    });

    menu.exec(m_treeView->viewport()->mapToGlobal(pos));
}

void ProjectExplorerWidget::applyTheme() {
    PCBTheme* theme = ThemeManager::theme();
    if (!theme) return;

    QString bg = theme->panelBackground().name();
    QString fg = theme->textColor().name();
    QString border = theme->panelBorder().name();
    QString accent = theme->accentColor().name();
    QString inputBg = (theme->type() == PCBTheme::Light) ? "#f1f5f9" : "#1a1a1a";
    QString hoverBg = (theme->type() == PCBTheme::Light) ? "#e7f0ff" : "#2a2a2f";
    QString altRow = (theme->type() == PCBTheme::Light) ? "#eef2f7" : "#202025";
    QString selectionBg = (theme->type() == PCBTheme::Light) ? "#cfe2ff" : "#1f2a44";

    setStyleSheet(QString(
        "QWidget { background-color: %1; color: %2; }"
        "QWidget#ExplorerHeader { background-color: %1; border-bottom: 1px solid %3; }"
        "QLineEdit { background-color: %5; border: 1px solid %3; padding: 4px 8px; border-radius: 4px; color: %2; }"
        "QLineEdit:focus { border: 1px solid %4; background-color: %1; }"
        "QTreeView { background-color: %1; border: none; outline: none; alternate-background-color: %6; }"
        "QTreeView::item { padding: 4px 6px; border-radius: 4px; margin: 0px 4px; height: 24px; }"
        "QTreeView::item:hover { background-color: %7; }"
        "QTreeView::item:selected { background-color: %8; color: %2; }"
        "QTreeView::branch:has-children:!has-siblings:closed, QTreeView::branch:closed:has-children:has-siblings { image: url(:/icons/chevron_right.svg); }"
        "QTreeView::branch:open:has-children:!has-siblings, QTreeView::branch:open:has-children:has-siblings { image: url(:/icons/chevron_down.svg); }"
    ).arg(bg, fg, border, accent, inputBg, altRow, hoverBg, selectionBg));

    if (m_titleLabel) {
        m_titleLabel->setStyleSheet(QString("font-weight: bold; font-size: 10px; color: %1; text-transform: uppercase; letter-spacing: 0.5px;")
                                    .arg((theme->type() == PCBTheme::Light) ? "#64748b" : "#94a3b8"));
    }
}
