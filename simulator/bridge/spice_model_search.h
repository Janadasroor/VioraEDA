#ifndef SPICE_MODEL_SEARCH_H
#define SPICE_MODEL_SEARCH_H

#include <QString>
#include <QVector>
#include <QPair>
#include "model_library_manager.h"

struct SpiceModelSearch {
    struct ScoredModel {
        SpiceModelInfo info;
        double score;
    };

    // Smart search with semantic keywords
    static QVector<ScoredModel> search(const QString& query, const QString& typeFilter);

private:
    static void applyKeywordScore(const QString& keyword, const SpiceModelInfo& info, double& score);
    static double parseParam(const SpiceModelInfo& info, const QString& key, double defaultValue = 0.0);
    static double toScientific(const QString& str);
};

#endif // SPICE_MODEL_SEARCH_H
