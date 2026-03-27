#ifndef SYMBOL_LIST_MODEL_H
#define SYMBOL_LIST_MODEL_H

#include <QAbstractItemModel>
#include <QVector>
#include <QStringList>
#include "../symbols/models/symbol_definition.h"

using Flux::Model::SymbolDefinition;

class SymbolListModel : public QAbstractItemModel {
    Q_OBJECT
public:
    enum SymbolRoles {
        NameRole = Qt::UserRole + 1,
        LibraryRole,
        CategoryRole,
        IsCategoryRole
    };

    struct Item {
        QString name;
        QString library;
        QString category;
        bool isCategory = false;
        int parentIdx = -1;
        QVector<int> children;
        // SymbolDefinition is now fetched on demand to save memory/startup time
    };

    explicit SymbolListModel(QObject* parent = nullptr);

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    struct SymbolMetadata {
        QString name;
        QString category;
        QString library;
    };

    void setSymbols(const QVector<SymbolMetadata>& builtIn, const QMap<QString, QStringList>& libraries);

    SymbolDefinition symbolDefinition(const QModelIndex& index) const;

private:
    QVector<Item> m_items;
    QVector<int> m_rootItems;
};

#endif // SYMBOL_LIST_MODEL_H
