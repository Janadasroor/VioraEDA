#include "spice_model_manager_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include "../core/theme_manager.h"
#include "../core/config_manager.h"
#include <QTextBrowser>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QApplication>

// --- Custom Delegate for Search Highlighting ---
class SearchHighlightDelegate : public QStyledItemDelegate {
public:
    explicit SearchHighlightDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void setSearchString(const QString& text) {
        m_searchString = text;
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        painter->save();
        
        // Draw standard background and focus
        QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
        opt.text = ""; // clear text for base draw, we draw it manually
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

        // Draw custom text with highlight
        QString text = index.data(Qt::DisplayRole).toString();
        
        // For QTreeView, we need to handle the rect slightly differently if it's multi-column
        QRect textRect = opt.rect.adjusted(8, 0, -8, 0);
        
        // If selection is active, use white text, otherwise standard
        QColor textColor = (opt.state & QStyle::State_Selected) ? Qt::white : QColor("#d4d4d8");

        painter->setClipRect(opt.rect);

        if (m_searchString.isEmpty()) {
            painter->setPen(textColor);
            painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);
        } else {
            int pos = 0;
            int lastPos = 0;
            int xOffset = textRect.left();
            QFontMetrics fm(opt.font);

            while ((pos = text.indexOf(m_searchString, lastPos, Qt::CaseInsensitive)) != -1) {
                QString before = text.mid(lastPos, pos - lastPos);
                if (!before.isEmpty()) {
                    painter->setPen(textColor);
                    painter->drawText(xOffset, textRect.top(), fm.horizontalAdvance(before), textRect.height(), Qt::AlignVCenter | Qt::AlignLeft, before);
                    xOffset += fm.horizontalAdvance(before);
                }

                QString match = text.mid(pos, m_searchString.length());
                int matchWidth = fm.horizontalAdvance(match);
                QRect highlightRect(xOffset, textRect.top() + (textRect.height() - fm.height()) / 2, matchWidth, fm.height());
                
                painter->fillRect(highlightRect, QColor(202, 138, 4));
                painter->setPen(Qt::white);
                painter->drawText(highlightRect, Qt::AlignVCenter | Qt::AlignLeft, match);
                xOffset += matchWidth;

                lastPos = pos + m_searchString.length();
            }

            QString after = text.mid(lastPos);
            if (!after.isEmpty()) {
                painter->setPen(textColor);
                painter->drawText(QRect(xOffset, textRect.top(), fm.horizontalAdvance(after) + 10, textRect.height()), Qt::AlignVCenter | Qt::AlignLeft, after);
            }
        }
        
        painter->restore();
    }

private:
    QString m_searchString;
};

// ---------------------------------------------

SpiceModelManagerDialog::SpiceModelManagerDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("SPICE Model Manager");
    setMinimumSize(850, 600);
    
    m_model = new SpiceModelListModel(this);
    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setFilterKeyColumn(-1); // Search all columns
    
    setupUI();
    
    connect(&ModelLibraryManager::instance(), &ModelLibraryManager::libraryReloaded, 
            this, [this]() { m_model->setModels(ModelLibraryManager::instance().allModels()); });
    
    m_model->setModels(ModelLibraryManager::instance().allModels());
}

void SpiceModelManagerDialog::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Header / Toolbar
    auto* toolbar = new QWidget;
    toolbar->setFixedHeight(50);
    toolbar->setStyleSheet("background-color: #1a1c22; border-bottom: 1px solid #2d2d30;");
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(15, 0, 15, 0);

    m_searchField = new QLineEdit;
    m_searchField->setPlaceholderText("Search models by name, type, or manufacturer...");
    m_searchField->setFixedHeight(32);
    m_searchField->setStyleSheet("background-color: #0a0a0c; border: 1px solid #3f3f46; border-radius: 6px; padding: 0 10px; color: white;");
    connect(m_searchField, &QLineEdit::textChanged, this, &SpiceModelManagerDialog::onSearchChanged);
    toolbarLayout->addWidget(m_searchField, 1);

    m_reloadBtn = new QPushButton("Reload Libraries");
    m_reloadBtn->setFixedHeight(32);
    m_reloadBtn->setStyleSheet("QPushButton { background-color: #27272a; border: 1px solid #3f3f46; border-radius: 6px; color: #d4d4d8; padding: 0 15px; } "
                              "QPushButton:hover { background-color: #3f3f46; }");
    connect(m_reloadBtn, &QPushButton::clicked, this, &SpiceModelManagerDialog::onReloadLibraries);
    toolbarLayout->addWidget(m_reloadBtn);

    auto* addPathBtn = new QPushButton("Add Path...");
    addPathBtn->setIcon(QIcon(":/icons/toolbar_new.png")); // Apply + icon if available
    addPathBtn->setFixedHeight(32);
    addPathBtn->setStyleSheet("QPushButton { background-color: #007acc; border: none; border-radius: 6px; color: white; padding: 0 15px; font-weight: bold; } "
                             "QPushButton:hover { background-color: #008be5; }");
    connect(addPathBtn, &QPushButton::clicked, this, &SpiceModelManagerDialog::onAddLibraryPath);
    toolbarLayout->addWidget(addPathBtn);

    mainLayout->addWidget(toolbar);

    // Modern Scrollbar Stylesheet
    QString scrollbarStyle = R"(
        QScrollBar:vertical {
            border: none;
            background: #0f1012;
            width: 10px;
            margin: 0px 0px 0px 0px;
        }
        QScrollBar::handle:vertical {
            background: #3f3f46;
            min-height: 20px;
            border-radius: 5px;
        }
        QScrollBar::handle:vertical:hover {
            background: #52525b;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
    )";
    this->setStyleSheet(scrollbarStyle);

    // Main Splitter
    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setChildrenCollapsible(false);
    splitter->setStyleSheet("QSplitter::handle { background-color: #2d2d30; width: 1px; }");

    // Left Panel: Model List
    m_modelView = new QTreeView;
    m_modelView->setModel(m_proxyModel);
    m_modelView->setUniformRowHeights(true);
    m_modelView->setSortingEnabled(true);
    m_modelView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_modelView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_modelView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_modelView->setIndentation(0);
    m_modelView->header()->setStretchLastSection(true);
    
    m_modelView->setStyleSheet("QTreeView { background-color: #0a0a0c; border: none; color: #d4d4d8; outline: 0; }"
                              "QTreeView::item { padding: 8px; border-bottom: 1px solid #1f1f23; }"
                              "QTreeView::item:selected { background-color: #1e3a8a; }");
    
    connect(m_modelView->selectionModel(), &QItemSelectionModel::currentChanged, 
            this, [this](const QModelIndex& current, const QModelIndex&) { onModelSelected(current); });
    
    // Assign custom highlighter delegate
    m_modelView->setItemDelegate(new SearchHighlightDelegate(m_modelView));
    splitter->addWidget(m_modelView);

    // Right Panel: Details
    auto* detailsPanel = new QWidget;
    detailsPanel->setStyleSheet("background-color: #0f1012;");
    auto* detailsLayout = new QVBoxLayout(detailsPanel);
    detailsLayout->setContentsMargins(25, 25, 25, 25);
    detailsLayout->setSpacing(15);

    m_modelTitle = new QLabel("Select a model to view details");
    m_modelTitle->setStyleSheet("font-size: 24px; font-weight: 800; color: white;");
    detailsLayout->addWidget(m_modelTitle);

    m_modelMeta = new QLabel("");
    m_modelMeta->setStyleSheet("color: #007acc; font-weight: bold; font-family: monospace;");
    detailsLayout->addWidget(m_modelMeta);

    auto* detailHeader = new QLabel("PARAMETERS AND MAPPING");
    detailHeader->setStyleSheet("font-size: 10px; font-weight: 800; color: #71717a; letter-spacing: 1.5px;");
    detailsLayout->addWidget(detailHeader);

    m_modelDetails = new QTextBrowser;
    m_modelDetails->setOpenExternalLinks(true);
    m_modelDetails->setStyleSheet("QTextBrowser { background-color: #0a0a0c; border: 1px solid #2d2d30; border-radius: 8px; padding: 15px; font-family: 'Inter', 'Segoe UI', sans-serif; }");
    detailsLayout->addWidget(m_modelDetails);

    splitter->addWidget(detailsPanel);
    splitter->setSizes({400, 450});

    mainLayout->addWidget(splitter);
}


void SpiceModelManagerDialog::onSearchChanged(const QString& query) {
    if (auto* delegate = dynamic_cast<SearchHighlightDelegate*>(m_modelView->itemDelegate())) {
        delegate->setSearchString(query);
    }
    m_proxyModel->setFilterFixedString(query);
    m_modelView->viewport()->update();
}

void SpiceModelManagerDialog::onModelSelected(const QModelIndex& index) {
    if (!index.isValid()) return;
    
    // Map proxy index to source index
    QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
    const auto& info = m_model->modelInfo(sourceIndex.row());
    
    m_modelTitle->setText(info.name);
    m_modelMeta->setText(""); // Replaced by HTML inside m_modelDetails
    
    QString html = "<html><head><style>"
                   "body { color: #d4d4d8; font-family: 'Inter', sans-serif; font-size: 13px; }"
                   ".badge { background-color: #1e40af; color: #eff6ff; padding: 3px 8px; border-radius: 4px; font-weight: 600; margin-right: 10px; }"
                   ".source { color: #60a5fa; text-decoration: none; font-weight: 500; font-family: 'JetBrains Mono', monospace; }"
                   ".params-grid { margin-top: 20px; display: grid; grid-template-columns: repeat(2, 1fr); gap: 8px; font-family: 'JetBrains Mono', monospace; }"
                   ".param-item { background: #18181b; padding: 6px 12px; border-radius: 4px; border: 1px solid #27272a; color: #a1a1aa; }"
                   "</style></head><body>";
    
    html += QString("<div><span class='badge'>%1</span> <span style='color: #71717a;'>Source:</span> <span class='source'>%2</span></div>")
                .arg(info.type, QFileInfo(info.libraryPath).fileName());
    
    html += "<div style='margin-top: 20px; font-size: 11px; font-weight: 800; color: #71717a; letter-spacing: 1.5px;'>PARAMETERS</div>";
    
    if (info.type == "Subcircuit") {
        html += QString("<div class='param-item' style='margin-top: 10px; color: #fff;'>%1</div>").arg(info.description);
    } else {
        html += "<div class='params-grid' style='margin-top: 10px;'>";
        for (int i = 0; i < info.params.size(); ++i) {
            html += QString("<div style='background: #18181b; padding: 6px 12px; border-radius: 4px; border: 1px solid #27272a; color: #a1a1aa; font-family: monospace; display: inline-block; margin: 4px;'>%1</div>").arg(info.params[i]);
        }
        html += "</div>";
    }
    
    html += "</body></html>";
    m_modelDetails->setHtml(html);
}

void SpiceModelManagerDialog::onReloadLibraries() {
    ModelLibraryManager::instance().reload();
    QMessageBox::information(this, "Success", "Libraries reloaded successfully.");
}

void SpiceModelManagerDialog::onAddLibraryPath() {
    QString dir = QFileDialog::getExistingDirectory(this, "Add Library Path", QDir::homePath());
    if (!dir.isEmpty()) {
        QStringList paths = ConfigManager::instance().modelPaths();
        if (!paths.contains(dir)) {
            paths.append(dir);
            ConfigManager::instance().setModelPaths(paths);
            onReloadLibraries();
        }
    }
}
