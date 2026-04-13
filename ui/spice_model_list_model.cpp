#include "spice_model_list_model.h"
#include <QFileInfo>
#include <QSettings>
#include <QMap>
#include <QSet>

SpiceModelListModel::SpiceModelListModel(QObject* parent)
    : QAbstractListModel(parent) {}

int SpiceModelListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return m_models.size();
}

int SpiceModelListModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return 3;
}

#include <QIcon>

QVariant SpiceModelListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_models.size())
        return QVariant();

    const auto& info = m_models.at(index.row());

    if (role == Qt::DecorationRole || role == IconRole) {
        if (index.column() != 0 && role == Qt::DecorationRole) return QVariant();

        if (info.type == "Subcircuit") {
            return QIcon(":/icons/comp_ic.svg");
        } else if (info.type == "NMOS" || info.type == "PMOS" || info.type == "NPN" || info.type == "PNP") {
            return QIcon(":/icons/comp_transistor.svg");
        } else {
            return QIcon(":/icons/comp_diode.svg");
        }
    }

    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        if (index.column() == 0) return info.name;
        if (index.column() == 1) return info.type;
        if (index.column() == 2) return QFileInfo(info.libraryPath).fileName();
        return QVariant();
    case TypeRole:
        return info.type;
    case LibraryPathRole:
        return info.libraryPath;
    case LibraryFileNameRole:
        return QFileInfo(info.libraryPath).fileName();
    case DescriptionRole:
        return info.description;
    case ParamsRole:
        return info.params;
    case UsedInSchematicRole:
        return m_usedModels.contains(info.name);
    case IsDuplicateRole:
        return m_duplicates.contains(info.name);
    case IsFavoriteRole:
        return m_favorites.contains(info.name);
    default:
        return QVariant();
    }
}

QVariant SpiceModelListModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch (section) {
    case 0: return "Model Name";
    case 1: return "Type";
    case 2: return "Library";
    default: return QVariant();
    }
}

void SpiceModelListModel::setModels(const QVector<SpiceModelInfo>& models) {
    beginResetModel();
    m_models = models;
    endResetModel();
    detectDuplicates();
}

const SpiceModelInfo& SpiceModelListModel::modelInfo(int row) const {
    return m_models.at(row);
}

void SpiceModelListModel::setFavorites(const QSet<QString>& favs) {
    m_favorites = favs;
    Q_EMIT dataChanged(index(0, 0), index(m_models.size() - 1, 0));
}

void SpiceModelListModel::toggleFavorite(const QString& modelName) {
    if (m_favorites.contains(modelName)) {
        m_favorites.remove(modelName);
    } else {
        m_favorites.insert(modelName);
    }
    // Update all rows with this model name
    for (int i = 0; i < m_models.size(); ++i) {
        if (m_models[i].name == modelName) {
            Q_EMIT dataChanged(index(i, 0), index(i, columnCount() - 1), {IsFavoriteRole});
        }
    }
}

bool SpiceModelListModel::isFavorite(const QString& modelName) const {
    return m_favorites.contains(modelName);
}

QSet<QString> SpiceModelListModel::favorites() const {
    return m_favorites;
}

void SpiceModelListModel::setUsedModels(const QSet<QString>& used) {
    beginResetModel();
    m_usedModels = used;
    endResetModel();
}

void SpiceModelListModel::detectDuplicates() {
    m_duplicates.clear();
    QMap<QString, int> nameCount;
    for (const auto& m : m_models) {
        nameCount[m.name]++;
    }
    for (auto it = nameCount.begin(); it != nameCount.end(); ++it) {
        if (it.value() > 1) {
            m_duplicates.insert(it.key());
        }
    }
    Q_EMIT dataChanged(index(0, 0), index(m_models.size() - 1, 0), {IsDuplicateRole});
}

SpiceModelListModel::Stats SpiceModelListModel::statistics() const {
    Stats stats;
    stats.total = m_models.size();
    for (const auto& m : m_models) {
        stats.byType[m.type]++;
    }
    return stats;
}
