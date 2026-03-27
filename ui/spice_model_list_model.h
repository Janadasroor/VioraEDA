#ifndef SPICE_MODEL_LIST_MODEL_H
#define SPICE_MODEL_LIST_MODEL_H

#include <QAbstractListModel>
#include <QVector>
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
        IconRole
    };

    explicit SpiceModelListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setModels(const QVector<SpiceModelInfo>& models);
    const SpiceModelInfo& modelInfo(int row) const;

private:
    QVector<SpiceModelInfo> m_models;
};

#endif // SPICE_MODEL_LIST_MODEL_H
