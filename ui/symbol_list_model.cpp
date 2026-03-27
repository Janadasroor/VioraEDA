#include "symbol_list_model.h"
#include "../symbols/symbol_library.h"
#include <QIcon>
#include <QFont>

SymbolListModel::SymbolListModel(QObject* parent)
    : QAbstractItemModel(parent) {}

QModelIndex SymbolListModel::index(int row, int column, const QModelIndex& parent) const {
    if (!hasIndex(row, column, parent)) return QModelIndex();

    if (!parent.isValid()) {
        if (row < m_rootItems.size())
            return createIndex(row, column, (const void*)&m_items[m_rootItems[row]]);
    } else {
        const Item* parentItem = static_cast<const Item*>(parent.internalPointer());
        if (row < parentItem->children.size())
            return createIndex(row, column, (const void*)&m_items[parentItem->children[row]]);
    }

    return QModelIndex();
}

QModelIndex SymbolListModel::parent(const QModelIndex& child) const {
    if (!child.isValid()) return QModelIndex();

    const Item* childItem = static_cast<const Item*>(child.internalPointer());
    if (childItem->parentIdx == -1) return QModelIndex();

    const Item* parentItem = &m_items[childItem->parentIdx];
    
    // Find parent's row in ITS parent
    int row = 0;
    int parentIdxInItems = childItem->parentIdx;
    
    if (parentItem->parentIdx == -1) {
        row = m_rootItems.indexOf(parentIdxInItems);
    } else {
        row = m_items[parentItem->parentIdx].children.indexOf(parentIdxInItems);
    }

    return createIndex(row, 0, (const void*)parentItem);
}

int SymbolListModel::rowCount(const QModelIndex& parent) const {
    if (parent.column() > 0) return 0;

    if (!parent.isValid()) {
        return m_rootItems.size();
    } else {
        const Item* parentItem = static_cast<const Item*>(parent.internalPointer());
        return parentItem->children.size();
    }
}

int SymbolListModel::columnCount(const QModelIndex& parent) const {
    return 1;
}

QVariant SymbolListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) return QVariant();

    const Item* item = static_cast<const Item*>(index.internalPointer());

    if (role == Qt::DisplayRole || role == NameRole) {
        return item->name;
    }

    if (role == Qt::DecorationRole) {
        if (item->isCategory) {
            return QIcon(":/icons/folder_open.svg");
        } else {
            return QIcon(":/icons/component_file.svg");
        }
    }

    if (role == Qt::FontRole && item->isCategory) {
        QFont font;
        font.setBold(true);
        font.setPointSize(10);
        return font;
    }

    if (role == IsCategoryRole) return item->isCategory;
    if (role == LibraryRole) return item->library;
    if (role == CategoryRole) return item->category;

    return QVariant();
}

QVariant SymbolListModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section == 0) {
        return "Component";
    }
    return QVariant();
}

void SymbolListModel::setSymbols(const QVector<SymbolMetadata>& builtIn, const QMap<QString, QStringList>& libraries) {
    beginResetModel();
    m_items.clear();
    m_rootItems.clear();

    // Estimate total size to minimize reallocations
    int totalSymbols = builtIn.size();
    for (const auto& names : libraries) totalSymbols += names.size();
    m_items.reserve(totalSymbols + libraries.size() + 10); // + categories

    // 1. Built-in Tools
    QMap<QString, int> builtInCategories;
    for (const auto& sym : builtIn) {
        QString catName = sym.category;
        if (catName.isEmpty()) catName = "Other";
        
        if (!builtInCategories.contains(catName)) {
            Item catItem;
            catItem.name = catName;
            catItem.isCategory = true;
            m_items.append(catItem);
            int catIdx = m_items.size() - 1;
            m_rootItems.append(catIdx);
            builtInCategories[catName] = catIdx;
        }

        Item symItem;
        symItem.name = sym.name;
        symItem.category = catName;
        symItem.parentIdx = builtInCategories[catName];
        m_items.append(symItem);
        m_items[symItem.parentIdx].children.append(m_items.size() - 1);
    }

    // 2. Library Symbols
    for (auto it = libraries.begin(); it != libraries.end(); ++it) {
        Item libItem;
        libItem.name = it.key().toUpper();
        libItem.isCategory = true;
        m_items.append(libItem);
        int libIdx = m_items.size() - 1;
        m_rootItems.append(libIdx);

        for (const auto& symName : it.value()) {
            Item symItem;
            symItem.name = symName;
            symItem.library = it.key();
            symItem.parentIdx = libIdx;
            m_items.append(symItem);
            m_items[libIdx].children.append(m_items.size() - 1);
        }
    }

    endResetModel();
}

SymbolDefinition SymbolListModel::symbolDefinition(const QModelIndex& index) const {
    if (!index.isValid()) return SymbolDefinition();

    const Item* item = static_cast<const Item*>(index.internalPointer());
    if (item->isCategory) return SymbolDefinition();

    // Lazy fetch from library manager
    SymbolDefinition* def = nullptr;
    if (!item->library.isEmpty()) {
        def = SymbolLibraryManager::instance().findSymbol(item->name, item->library);
    } else {
        def = SymbolLibraryManager::instance().findSymbol(item->name);
    }
    
    return def ? *def : SymbolDefinition(item->name);
}
