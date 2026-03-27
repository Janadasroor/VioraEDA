#include "spice_model_list_model.h"
#include <QFileInfo>

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
}

const SpiceModelInfo& SpiceModelListModel::modelInfo(int row) const {
    return m_models.at(row);
}
