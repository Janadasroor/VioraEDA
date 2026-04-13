#ifndef SPICE_MODEL_LIST_MODEL_H
#define SPICE_MODEL_LIST_MODEL_H

#include <QAbstractListModel>
#include <QVector>
#include <QSet>
#include "../simulator/bridge/model_library_manager.h"

class SpiceModelListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum ModelRoles {
        NameRole = Qt::UserRole + 1,
        TypeRole,
        LibraryPathRole,
        LibraryFileNameRole,
        DescriptionRole,
        ParamsRole,
        IconRole,
        UsedInSchematicRole,  // bool: model is referenced in current schematic
        IsDuplicateRole,      // bool: multiple models share same name
        IsFavoriteRole        // bool: user-pinned favorite
    };

    enum ColumnId { ColName = 0, ColType, ColLibrary, ColFavorites };

    explicit SpiceModelListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setModels(const QVector<SpiceModelInfo>& models);
    const SpiceModelInfo& modelInfo(int row) const;

    // Favorite management
    void setFavorites(const QSet<QString>& favs);
    void toggleFavorite(const QString& modelName);
    bool isFavorite(const QString& modelName) const;
    QSet<QString> favorites() const;

    // Schematic usage tracking
    void setUsedModels(const QSet<QString>& used);

    // Duplicate detection
    void detectDuplicates();
    bool isDuplicate(const QString& modelName) const { return m_duplicates.contains(modelName); }

    // Statistics
    struct Stats {
        int total = 0;
        QMap<QString, int> byType;
    };
    Stats statistics() const;

private:
    QVector<SpiceModelInfo> m_models;
    QSet<QString> m_favorites;
    QSet<QString> m_usedModels;
    QSet<QString> m_duplicates;
};

#endif // SPICE_MODEL_LIST_MODEL_H
