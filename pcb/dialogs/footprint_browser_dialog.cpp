#include "footprint_browser_dialog.h"
#include "../../footprints/footprint_library.h"
#include "../../core/theme_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QHeaderView>
#include <QApplication>

FootprintBrowserDialog::FootprintBrowserDialog(QWidget *parent)
    : QDialog(parent) {
    setWindowTitle("Footprint Library Browser");
    resize(1000, 700);
    setupUI();
    performSearch("");
}

FootprintBrowserDialog::~FootprintBrowserDialog() {}

void FootprintBrowserDialog::setupUI() {
    PCBTheme* theme = ThemeManager::theme();
    QString accent = theme ? theme->accentColor().name() : "#007acc";
    QString border = theme ? theme->panelBorder().name() : "#cccccc";

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(12);

    // Search Area
    QHBoxLayout* searchLayout = new QHBoxLayout();
    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText("Search footprints (e.g. 0805, SOIC, SOT-23)...");
    m_searchBox->setFixedHeight(40);
    
    QPushButton* searchBtn = new QPushButton("Search", this);
    searchBtn->setFixedHeight(40);
    searchBtn->setFixedWidth(100);
    searchBtn->setStyleSheet(QString("background-color: %1; color: white; border: none; font-weight: bold;").arg(accent));
    
    connect(m_searchBox, &QLineEdit::returnPressed, this, &FootprintBrowserDialog::onSearch);
    connect(searchBtn, &QPushButton::clicked, this, &FootprintBrowserDialog::onSearch);

    searchLayout->addWidget(m_searchBox);
    searchLayout->addWidget(searchBtn);
    mainLayout->addLayout(searchLayout);

    // Center Splitter
    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
    
    m_librariesTree = new QTreeWidget(this);
    m_librariesTree->setHeaderLabel("Libraries");
    connect(m_librariesTree, &QTreeWidget::itemClicked, this, &FootprintBrowserDialog::onCategorySelected);
    
    QTreeWidgetItem* builtInRoot = new QTreeWidgetItem(m_librariesTree, QStringList() << "Built-in Libraries");
    builtInRoot->setExpanded(true);
    
    QTreeWidgetItem* userRoot = new QTreeWidgetItem(m_librariesTree, QStringList() << "User Libraries");
    userRoot->setExpanded(true);

    auto libs = FootprintLibraryManager::instance().libraries();
    for (FootprintLibrary* lib : libs) {
        new QTreeWidgetItem(lib->isBuiltIn() ? builtInRoot : userRoot, QStringList() << lib->name());
    }
    
    m_resultsList = new QListWidget(this);
    connect(m_resultsList, &QListWidget::itemClicked, this, &FootprintBrowserDialog::onResultSelected);

    // Preview Panel
    QWidget* previewPanel = new QWidget(this);
    previewPanel->setFixedWidth(350);
    QVBoxLayout* previewLayout = new QVBoxLayout(previewPanel);
    previewLayout->setContentsMargins(15, 0, 0, 0);
    previewLayout->setSpacing(10);

    m_previewTitle = new QLabel("Select a Footprint", this);
    m_previewTitle->setStyleSheet("font-size: 18px; font-weight: bold;");
    m_previewTitle->setWordWrap(true);
    
    m_previewDesc = new QLabel("View details and footprint preview here.", this);
    m_previewDesc->setStyleSheet("color: #666; font-size: 13px;");
    m_previewDesc->setWordWrap(true);

    m_previewStats = new QLabel("", this);
    m_previewStats->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold;").arg(accent));

    m_previewView = new FootprintPreviewView(this);
    m_previewView->setFixedHeight(300);
    m_previewView->setStyleSheet(QString("background-color: %1; border: 1px solid %2; border-radius: 4px;").arg((theme && theme->type() == PCBTheme::Light) ? "#f8fafc" : "#0d1117", border));

    QPushButton* insertBtn = new QPushButton("Insert into PCB", this);
    insertBtn->setFixedHeight(44);
    insertBtn->setStyleSheet(QString("background-color: %1; color: white; font-weight: bold; border-radius: 4px;").arg(accent));
    connect(insertBtn, &QPushButton::clicked, this, &FootprintBrowserDialog::onInsertClicked);

    previewLayout->addWidget(m_previewTitle);
    previewLayout->addWidget(m_previewDesc);
    previewLayout->addWidget(m_previewStats);
    previewLayout->addWidget(m_previewView);
    previewLayout->addStretch();
    previewLayout->addWidget(insertBtn);

    splitter->addWidget(m_librariesTree);
    splitter->addWidget(m_resultsList);
    splitter->addWidget(previewPanel);
    splitter->setStretchFactor(1, 2); 
    
    mainLayout->addWidget(splitter);
}

void FootprintBrowserDialog::performSearch(const QString& query) {
    m_resultsList->clear();
    m_searchResults.clear();

    auto libs = FootprintLibraryManager::instance().libraries();
    for (FootprintLibrary* lib : libs) {
        for (const QString& name : lib->getFootprintNames()) {
            if (query.isEmpty() || name.contains(query, Qt::CaseInsensitive)) {
                FootprintDefinition fp = lib->getFootprint(name);
                m_searchResults.append(fp);
                
                QListWidgetItem* item = new QListWidgetItem(m_resultsList);
                item->setText(fp.name());
                item->setToolTip(fp.description());
            }
        }
    }
}

void FootprintBrowserDialog::onSearch() {
    performSearch(m_searchBox->text());
}

void FootprintBrowserDialog::onCategorySelected(QTreeWidgetItem* item, int) {
    if (item->childCount() > 0) return; // Root items
    
    m_resultsList->clear();
    m_searchResults.clear();
    
    FootprintLibrary* lib = FootprintLibraryManager::instance().findLibrary(item->text(0));
    if (lib) {
        for (const QString& name : lib->getFootprintNames()) {
            FootprintDefinition fp = lib->getFootprint(name);
            m_searchResults.append(fp);
            
            QListWidgetItem* item = new QListWidgetItem(m_resultsList);
            item->setText(fp.name());
        }
    }
}

void FootprintBrowserDialog::onResultSelected(QListWidgetItem* item) {
    int row = m_resultsList->row(item);
    if (row >= 0 && row < m_searchResults.size()) {
        m_selectedFootprint = m_searchResults[row];
        updatePreview();
    }
}

void FootprintBrowserDialog::updatePreview() {
    m_previewTitle->setText(m_selectedFootprint.name());
    m_previewDesc->setText(m_selectedFootprint.description().isEmpty() ? "No description available." : m_selectedFootprint.description());
    m_previewStats->setText(QString("%1 primitives • %2 pads").arg(m_selectedFootprint.primitives().size()).arg(m_selectedFootprint.padPositions().size()));

    if (m_selectedFootprint.isValid()) {
        m_previewView->setFootprint(m_selectedFootprint);
    } else {
        m_previewView->clear();
    }
}

void FootprintBrowserDialog::onInsertClicked() {
    if (!m_selectedFootprint.name().isEmpty()) {
        emit footprintSelected(m_selectedFootprint);
        accept();
    }
}
