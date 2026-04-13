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
#include <QTextStream>
#include <QFileInfo>
#include <QSettings>
#include "../core/theme_manager.h"
#include "../core/config_manager.h"
#include <QTextBrowser>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QApplication>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QCheckBox>
#include <QToolButton>

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

        QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
        opt.text = "";
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

        QString text = index.data(Qt::DisplayRole).toString();
        QRect textRect = opt.rect.adjusted(8, 0, -8, 0);
        QColor textColor = (opt.state & QStyle::State_Selected) ? Qt::white : QColor("#d4d4d8");

        // Draw favorite star
        bool isFav = index.data(SpiceModelListModel::IsFavoriteRole).toBool();
        bool isDup = index.data(SpiceModelListModel::IsDuplicateRole).toBool();
        bool isUsed = index.data(SpiceModelListModel::UsedInSchematicRole).toBool();

        int starX = textRect.left();
        if (isFav) {
            painter->setPen(QColor(251, 191, 36));
            painter->setBrush(QColor(251, 191, 36));
            painter->drawText(starX, textRect.top(), 14, textRect.height(), Qt::AlignVCenter | Qt::AlignLeft, "★");
            starX += 16;
        }
        if (isDup) {
            painter->setPen(QColor(239, 68, 68));
            painter->setBrush(QColor(239, 68, 68));
            painter->drawText(starX, textRect.top(), 14, textRect.height(), Qt::AlignVCenter | Qt::AlignLeft, "⚠");
            starX += 16;
        }
        if (isUsed) {
            painter->setPen(QColor(34, 197, 94));
            painter->setBrush(QColor(34, 197, 94));
            painter->drawText(starX, textRect.top(), 14, textRect.height(), Qt::AlignVCenter | Qt::AlignLeft, "✓");
            starX += 16;
        }

        painter->setClipRect(opt.rect);

        if (m_searchString.isEmpty()) {
            painter->setPen(textColor);
            painter->drawText(starX, textRect.top(), textRect.width() - (starX - textRect.left()), textRect.height(), Qt::AlignVCenter | Qt::AlignLeft, text);
        } else {
            int pos = 0;
            int lastPos = 0;
            int xOffset = starX;
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
    setMinimumSize(1000, 650);

    m_model = new SpiceModelListModel(this);
    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setFilterKeyColumn(-1);

    loadFavorites();

    setupUI();

    connect(&ModelLibraryManager::instance(), &ModelLibraryManager::libraryReloaded,
            this, [this]() {
                m_model->setModels(ModelLibraryManager::instance().allModels());
                m_model->setUsedModels(getUsedModelsFromSchematic());
                updateStatsLabel();
            });

    m_model->setModels(ModelLibraryManager::instance().allModels());
    m_model->setFavorites(m_favorites);
    m_model->setUsedModels(getUsedModelsFromSchematic());
    updateStatsLabel();
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

    // Advanced filter toggle
    auto* filterToggle = new QToolButton;
    filterToggle->setText("Filters ▾");
    filterToggle->setFixedHeight(32);
    filterToggle->setStyleSheet("QToolButton { background-color: #27272a; border: 1px solid #3f3f46; border-radius: 6px; color: #d4d4d8; padding: 0 10px; } QToolButton:hover { background-color: #3f3f46; }");
    m_advancedFilterPanel = new QWidget;
    m_advancedFilterPanel->hide();
    auto* advLayout = new QHBoxLayout(m_advancedFilterPanel);
    advLayout->setContentsMargins(15, 5, 15, 5);
    m_advancedFilterPanel->setStyleSheet("background-color: #1a1c22; border-bottom: 1px solid #2d2d30;");
    m_filterDuplicates = new QCheckBox("Duplicates only");
    m_filterFavorites = new QCheckBox("Favorites only");
    m_filterUsed = new QCheckBox("Used in schematic");
    m_typeFilter = new QLineEdit;
    m_typeFilter->setPlaceholderText("Filter by type (e.g. NMOS)");
    m_typeFilter->setFixedWidth(150);
    m_paramFilter = new QLineEdit;
    m_paramFilter->setPlaceholderText("Filter by param (e.g. Vto<2)");
    m_paramFilter->setFixedWidth(150);
    advLayout->addWidget(m_filterDuplicates);
    advLayout->addWidget(m_filterFavorites);
    advLayout->addWidget(m_filterUsed);
    advLayout->addWidget(m_typeFilter);
    advLayout->addWidget(m_paramFilter);
    connect(filterToggle, &QToolButton::clicked, [this]() { m_advancedFilterPanel->setVisible(!m_advancedFilterPanel->isVisible()); });
    connect(m_filterDuplicates, &QCheckBox::toggled, this, &SpiceModelManagerDialog::onAdvancedFilterChanged);
    connect(m_filterFavorites, &QCheckBox::toggled, this, &SpiceModelManagerDialog::onAdvancedFilterChanged);
    connect(m_filterUsed, &QCheckBox::toggled, this, &SpiceModelManagerDialog::onAdvancedFilterChanged);
    connect(m_typeFilter, &QLineEdit::textChanged, this, &SpiceModelManagerDialog::onAdvancedFilterChanged);
    connect(m_paramFilter, &QLineEdit::textChanged, this, &SpiceModelManagerDialog::onAdvancedFilterChanged);

    mainLayout->addWidget(toolbar);
    mainLayout->addWidget(m_advancedFilterPanel);

    m_reloadBtn = new QPushButton("Reload");
    m_reloadBtn->setFixedHeight(32);
    m_reloadBtn->setStyleSheet("QPushButton { background-color: #27272a; border: 1px solid #3f3f46; border-radius: 6px; color: #d4d4d8; padding: 0 15px; } QPushButton:hover { background-color: #3f3f46; }");
    connect(m_reloadBtn, &QPushButton::clicked, this, &SpiceModelManagerDialog::onReloadLibraries);
    toolbarLayout->addWidget(m_reloadBtn);

    auto* addPathBtn = new QPushButton("Add Path");
    addPathBtn->setFixedHeight(32);
    addPathBtn->setStyleSheet("QPushButton { background-color: #007acc; border: none; border-radius: 6px; color: white; padding: 0 15px; font-weight: bold; } QPushButton:hover { background-color: #008be5; }");
    connect(addPathBtn, &QPushButton::clicked, this, &SpiceModelManagerDialog::onAddLibraryPath);
    toolbarLayout->addWidget(addPathBtn);

    // Stats label
    m_statsLabel = new QLabel("");
    m_statsLabel->setStyleSheet("color: #71717a; font-size: 11px;");
    toolbarLayout->addWidget(m_statsLabel);

    // Modern Scrollbar Stylesheet
    QString scrollbarStyle = R"(
        QScrollBar:vertical { border: none; background: #0f1012; width: 10px; margin: 0px; }
        QScrollBar::handle:vertical { background: #3f3f46; min-height: 20px; border-radius: 5px; }
        QScrollBar::handle:vertical:hover { background: #52525b; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
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

    m_modelView->setStyleSheet("QTreeView { background-color: #0a0a0c; border: none; color: #d4d4d8; outline: 0; } QTreeView::item { padding: 8px; border-bottom: 1px solid #1f1f23; } QTreeView::item:selected { background-color: #1e3a8a; }");

    m_highlightDelegate = new SearchHighlightDelegate(m_modelView);
    m_modelView->setItemDelegate(m_highlightDelegate);

    connect(m_modelView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this](const QModelIndex& current, const QModelIndex&) { onModelSelected(current); });
    connect(m_modelView, &QTreeView::doubleClicked, this, &SpiceModelManagerDialog::onModelDoubleClicked);

    splitter->addWidget(m_modelView);

    // Right Panel: Details with Tabs
    auto* detailsPanel = new QWidget;
    detailsPanel->setStyleSheet("background-color: #0f1012;");
    auto* detailsLayout = new QVBoxLayout(detailsPanel);
    detailsLayout->setContentsMargins(20, 15, 20, 15);
    detailsLayout->setSpacing(10);

    // Action buttons row
    auto* actionRow = new QHBoxLayout;
    m_favBtn = new QPushButton("★ Favorite");
    m_favBtn->setFixedHeight(28);
    m_favBtn->setStyleSheet("QPushButton { background-color: #27272a; border: 1px solid #3f3f46; border-radius: 6px; color: #d4d4d8; padding: 0 12px; } QPushButton:hover { background-color: #3f3f46; }");
    connect(m_favBtn, &QPushButton::clicked, this, &SpiceModelManagerDialog::onToggleFavorite);

    m_applyBtn = new QPushButton("Apply to Selected");
    m_applyBtn->setFixedHeight(28);
    m_applyBtn->setStyleSheet("QPushButton { background-color: #007acc; border: none; border-radius: 6px; color: white; padding: 0 12px; font-weight: bold; } QPushButton:hover { background-color: #008be5; }");
    connect(m_applyBtn, &QPushButton::clicked, this, &SpiceModelManagerDialog::onApplyToSelectedComponent);

    m_exportBtn = new QPushButton("Export CSV");
    m_exportBtn->setFixedHeight(28);
    m_exportBtn->setStyleSheet("QPushButton { background-color: #27272a; border: 1px solid #3f3f46; border-radius: 6px; color: #d4d4d8; padding: 0 12px; } QPushButton:hover { background-color: #3f3f46; }");
    connect(m_exportBtn, &QPushButton::clicked, this, &SpiceModelManagerDialog::onExportCSV);

    actionRow->addWidget(m_favBtn);
    actionRow->addWidget(m_applyBtn);
    actionRow->addStretch();
    actionRow->addWidget(m_exportBtn);
    detailsLayout->addLayout(actionRow);

    // Tabs
    m_detailsTabs = new QTabWidget;
    m_detailsTabs->setStyleSheet("QTabWidget::pane { border: 1px solid #2d2d30; border-radius: 8px; background: #0a0a0c; } QTabBar::tab { background: #1a1c22; color: #71717a; padding: 8px 16px; border: none; border-bottom: 2px solid transparent; } QTabBar::tab:selected { color: white; border-bottom: 2px solid #007acc; }");

    // Tab 1: Details
    auto* detailsTab = new QWidget;
    auto* detailsTabLayout = new QVBoxLayout(detailsTab);
    detailsTabLayout->setContentsMargins(15, 15, 15, 15);

    m_modelTitle = new QLabel("Select a model to view details");
    m_modelTitle->setStyleSheet("font-size: 22px; font-weight: 800; color: white;");
    detailsTabLayout->addWidget(m_modelTitle);

    m_modelMeta = new QLabel("");
    m_modelMeta->setStyleSheet("color: #007acc; font-weight: bold; font-family: monospace;");
    detailsTabLayout->addWidget(m_modelMeta);

    m_modelDetails = new QTextBrowser;
    m_modelDetails->setOpenExternalLinks(true);
    m_modelDetails->setStyleSheet("QTextBrowser { background-color: #0a0a0c; border: 1px solid #2d2d30; border-radius: 8px; padding: 15px; font-family: 'Inter', 'Segoe UI', sans-serif; }");
    detailsTabLayout->addWidget(m_modelDetails);

    m_detailsTabs->addTab(detailsTab, "Details");

    // Tab 2: Raw SPICE
    auto* rawTab = new QWidget;
    auto* rawTabLayout = new QVBoxLayout(rawTab);
    rawTabLayout->setContentsMargins(15, 15, 15, 15);
    m_rawSpicePreview = new QPlainTextEdit;
    m_rawSpicePreview->setReadOnly(true);
    m_rawSpicePreview->setStyleSheet("QPlainTextEdit { background-color: #0a0a0c; border: 1px solid #2d2d30; border-radius: 8px; padding: 15px; font-family: 'JetBrains Mono', 'Courier New', monospace; font-size: 12px; color: #a1a1aa; }");
    rawTabLayout->addWidget(m_rawSpicePreview);
    m_detailsTabs->addTab(rawTab, "Raw SPICE");

    // Tab 3: Pin Diagram
    auto* pinTab = new QWidget;
    auto* pinTabLayout = new QVBoxLayout(pinTab);
    pinTabLayout->setContentsMargins(15, 15, 15, 15);
    m_pinDiagramView = new QTextBrowser;
    m_pinDiagramView->setOpenExternalLinks(false);
    m_pinDiagramView->setStyleSheet("QTextBrowser { background-color: #0a0a0c; border: 1px solid #2d2d30; border-radius: 8px; padding: 15px; font-family: 'JetBrains Mono', monospace; }");
    pinTabLayout->addWidget(m_pinDiagramView);
    m_detailsTabs->addTab(pinTab, "Pinout");

    detailsLayout->addWidget(m_detailsTabs);

    splitter->addWidget(detailsPanel);
    splitter->setSizes({450, 550});

    mainLayout->addWidget(splitter);
}

void SpiceModelManagerDialog::loadFavorites() {
    QSettings settings("VioSpice", "ModelManager");
    auto favList = settings.value("favorites").toStringList();
    m_favorites = QSet<QString>(favList.begin(), favList.end());
    m_recentModels = settings.value("recentModels").toStringList();
}

void SpiceModelManagerDialog::saveFavorites() {
    QSettings settings("VioSpice", "ModelManager");
    settings.setValue("favorites", QStringList(m_favorites.values()));
    settings.setValue("recentModels", m_recentModels);
}

void SpiceModelManagerDialog::setUsedModels(const QSet<QString>& used) {
    m_model->setUsedModels(used);
}

QSet<QString> SpiceModelManagerDialog::getUsedModelsFromSchematic() const {
    // This would be populated by the caller (SchematicEditor)
    // For now return empty; caller can call setUsedModels()
    return QSet<QString>();
}

void SpiceModelManagerDialog::onSearchChanged(const QString& query) {
    if (m_highlightDelegate) {
        m_highlightDelegate->setSearchString(query);
    }
    m_proxyModel->setFilterFixedString(query);
    m_modelView->viewport()->update();
    updateStatsLabel();
}

void SpiceModelManagerDialog::onAdvancedFilterChanged() {
    bool dupOnly = m_filterDuplicates->isChecked();
    bool favOnly = m_filterFavorites->isChecked();
    bool usedOnly = m_filterUsed->isChecked();
    QString typeFilter = m_typeFilter->text().trimmed().toUpper();
    QString paramFilter = m_paramFilter->text().trimmed().toLower();

    m_proxyModel->setFilterFixedString(m_searchField->text());

    // Apply advanced filters by hiding/showing rows
    for (int i = 0; i < m_model->rowCount(); ++i) {
        QModelIndex idx = m_model->index(i, 0);
        bool show = true;
        if (dupOnly && !idx.data(SpiceModelListModel::IsDuplicateRole).toBool()) show = false;
        if (favOnly && !idx.data(SpiceModelListModel::IsFavoriteRole).toBool()) show = false;
        if (usedOnly && !idx.data(SpiceModelListModel::UsedInSchematicRole).toBool()) show = false;
        if (!typeFilter.isEmpty() && !idx.data(SpiceModelListModel::TypeRole).toString().toUpper().contains(typeFilter)) show = false;
        if (!paramFilter.isEmpty()) {
            auto params = idx.data(SpiceModelListModel::ParamsRole).toStringList();
            bool paramMatch = false;
            for (const auto& p : params) {
                if (p.toLower().contains(paramFilter)) { paramMatch = true; break; }
            }
            if (!paramMatch) show = false;
        }
        m_modelView->setRowHidden(i, m_proxyModel->mapFromSource(idx).parent(), !show);
    }
    updateStatsLabel();
}

void SpiceModelManagerDialog::onModelSelected(const QModelIndex& index) {
    if (!index.isValid()) return;
    QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
    updateDetailsPanel(sourceIndex);
}

void SpiceModelManagerDialog::onModelDoubleClicked(const QModelIndex& index) {
    if (!index.isValid()) return;
    QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
    const auto& info = m_model->modelInfo(sourceIndex.row());

    // Add to recent models
    m_recentModels.removeAll(info.name);
    m_recentModels.prepend(info.name);
    if (m_recentModels.size() > 20) m_recentModels.removeLast();
    saveFavorites();
}

void SpiceModelManagerDialog::updateDetailsPanel(const QModelIndex& sourceIndex) {
    if (!sourceIndex.isValid()) return;
    const auto& info = m_model->modelInfo(sourceIndex.row());

    m_modelTitle->setText(info.name);

    // Update favorite button text
    bool isFav = m_model->isFavorite(info.name);
    m_favBtn->setText(isFav ? "★ Favorited" : "☆ Favorite");

    QString html = "<html><head><style>"
                   "body { color: #d4d4d8; font-family: 'Inter', sans-serif; font-size: 13px; }"
                   ".badge { display: inline-block; padding: 3px 8px; border-radius: 4px; font-weight: 600; margin-right: 8px; font-size: 11px; }"
                   ".badge-type { background-color: #1e40af; color: #eff6ff; }"
                   ".badge-dup { background-color: #dc2626; color: white; }"
                   ".badge-used { background-color: #16a34a; color: white; }"
                   ".badge-fav { background-color: #d97706; color: white; }"
                   ".source { color: #60a5fa; font-weight: 500; font-family: 'JetBrains Mono', monospace; }"
                   ".params-grid { display: flex; flex-wrap: wrap; gap: 6px; margin-top: 10px; }"
                   ".param-item { background: #18181b; padding: 6px 12px; border-radius: 4px; border: 1px solid #27272a; color: #a1a1aa; font-family: 'JetBrains Mono', monospace; font-size: 12px; }"
                   "</style></head><body>";

    // Badges
    QStringList badges;
    badges << QString("<span class='badge badge-type'>%1</span>").arg(info.type);
    if (m_model->isDuplicate(info.name)) badges << "<span class='badge badge-dup'>⚠ Duplicate</span>";
    if (info.type != "Subcircuit") {
        // Show used/fav badges for primitive models
    }
    html += "<div style='margin-bottom: 12px;'>" + badges.join(" ") + "</div>";

    html += QString("<div style='margin-bottom: 16px;'><span style='color: #71717a;'>Source:</span> <span class='source'>%1</span></div>")
                .arg(QFileInfo(info.libraryPath).fileName());

    if (info.type == "Subcircuit") {
        html += QString("<div class='param-item' style='margin-top: 10px; color: #fff;'>%1</div>").arg(info.description);
    } else {
        html += "<div style='margin-top: 15px; font-size: 11px; font-weight: 800; color: #71717a; letter-spacing: 1.5px;'>PARAMETERS (" + QString::number(info.params.size()) + ")</div>";
        html += "<div class='params-grid'>";
        for (int i = 0; i < info.params.size(); ++i) {
            html += QString("<div class='param-item'>%1</div>").arg(info.params[i]);
        }
        html += "</div>";
    }

    html += "</body></html>";
    m_modelDetails->setHtml(html);

    // Raw SPICE preview
    m_rawSpicePreview->setPlainText(generateRawSpiceLine(info));

    // Pin diagram for subcircuits
    if (info.type == "Subcircuit") {
        m_pinDiagramView->setHtml(generatePinDiagram(info));
    } else {
        m_pinDiagramView->setHtml("<div style='color: #71717a; text-align: center; padding: 40px;'>Pin diagram not applicable for primitive models</div>");
    }
}

QString SpiceModelManagerDialog::generateRawSpiceLine(const SpiceModelInfo& info) const {
    if (info.type == "Subcircuit") {
        return QString(".subckt %1 %2\n* Parameters defined in subcircuit body\n.ends %3").arg(info.name, info.description, info.name);
    }

    QString line = QString(".model %1 %2(").arg(info.name, info.type);
    QStringList params;
    for (const auto& p : info.params) {
        params << p;
    }
    line += params.join(" ") + ")";
    return line;
}

QString SpiceModelManagerDialog::generatePinDiagram(const SpiceModelInfo& info) const {
    // Parse pin names from description (e.g., "5 pins" -> try to parse from params)
    QStringList pins;
    for (const auto& p : info.params) {
        if (p.startsWith("pin") || p.contains("pin", Qt::CaseInsensitive)) {
            pins << p;
        }
    }
    if (pins.isEmpty()) {
        // Fallback: try to extract from description
        pins = info.description.split(",", Qt::SkipEmptyParts);
        for (auto& p : pins) p = p.trimmed();
    }
    if (pins.isEmpty()) {
        return "<div style='color: #71717a; text-align: center; padding: 40px;'>No pin information available</div>";
    }

    QString html = "<html><body style='color: #d4d4d8; font-family: monospace;'>";
    html += "<div style='text-align: center; margin-bottom: 20px; color: #60a5fa; font-weight: bold;'>" + info.name + "</div>";
    html += "<div style='display: flex; justify-content: center; gap: 60px;'>";

    // Left pins
    html += "<div style='text-align: right;'>";
    for (int i = 0; i < pins.size() / 2; ++i) {
        html += QString("<div style='padding: 4px 0; color: #a1a1aa;'>%1 ─┤</div>").arg(pins[i]);
    }
    html += "</div>";

    // Component body
    html += "<div style='border: 2px solid #3f3f46; border-radius: 8px; padding: 20px 30px; background: #18181b;'>";
    html += "<div style='text-align: center; color: white; font-weight: bold;'>" + info.type + "</div>";
    html += "</div>";

    // Right pins
    html += "<div style='text-align: left;'>";
    for (int i = pins.size() / 2; i < pins.size(); ++i) {
        html += QString("<div style='padding: 4px 0; color: #a1a1aa;'>├─ %1</div>").arg(pins[i]);
    }
    html += "</div>";

    html += "</div></body></html>";
    return html;
}

void SpiceModelManagerDialog::onToggleFavorite() {
    QModelIndex current = m_modelView->currentIndex();
    if (!current.isValid()) return;
    QModelIndex sourceIndex = m_proxyModel->mapToSource(current);
    const auto& info = m_model->modelInfo(sourceIndex.row());

    m_model->toggleFavorite(info.name);
    if (m_model->isFavorite(info.name)) {
        m_favorites.insert(info.name);
    } else {
        m_favorites.remove(info.name);
    }
    saveFavorites();

    bool isFav = m_model->isFavorite(info.name);
    m_favBtn->setText(isFav ? "★ Favorited" : "☆ Favorite");
}

void SpiceModelManagerDialog::onApplyToSelectedComponent() {
    QModelIndex current = m_modelView->currentIndex();
    if (!current.isValid()) return;
    QModelIndex sourceIndex = m_proxyModel->mapToSource(current);
    const auto& info = m_model->modelInfo(sourceIndex.row());

    QMessageBox::information(this, "Apply Model",
        QString("Model '%1' would be applied to the selected component.\n\n"
                "To implement this, connect this signal to SchematicEditor::onAssignModel().")
            .arg(info.name));
}

void SpiceModelManagerDialog::onExportCSV() {
    QString filePath = QFileDialog::getSaveFileName(this, "Export Models", QDir::homePath(), "CSV Files (*.csv)");
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Could not write to file.");
        return;
    }

    QTextStream out(&file);
    out << "Name,Type,Library,Parameters\n";
    for (int i = 0; i < m_model->rowCount(); ++i) {
        const auto& info = m_model->modelInfo(i);
        out << QString("\"%1\",\"%2\",\"%3\",\"%4\"\n")
                   .arg(info.name, info.type, QFileInfo(info.libraryPath).fileName(), info.params.join("; ").replace("\"", "\"\""));
    }
    file.close();
    QMessageBox::information(this, "Success", QString("Exported %1 models to:\n%2").arg(m_model->rowCount()).arg(filePath));
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

void SpiceModelManagerDialog::updateStatsLabel() {
    auto stats = m_model->statistics();
    QStringList parts;
    parts << QString("%1 total").arg(stats.total);
    for (auto it = stats.byType.begin(); it != stats.byType.end(); ++it) {
        parts << QString("%1 %2").arg(it.value()).arg(it.key());
    }
    m_statsLabel->setText(parts.join("  •  "));
}

void SpiceModelManagerDialog::onShowStatistics() {
    auto stats = m_model->statistics();
    QString msg = QString("Total Models: %1\n\n").arg(stats.total);
    for (auto it = stats.byType.begin(); it != stats.byType.end(); ++it) {
        msg += QString("%1: %2\n").arg(it.key(), 15).arg(it.value());
    }
    QMessageBox::information(this, "Model Statistics", msg);
}
