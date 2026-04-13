#include "spice_model_search.h"
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

QVector<SpiceModelSearch::ScoredModel> SpiceModelSearch::search(const QString& query, const QString& typeFilter) {
    QVector<ScoredModel> results;
    const auto allModels = ModelLibraryManager::instance().allModels();
    const QString q = query.trimmed().toLower();

    for (const auto& mi : allModels) {
        if (!typeFilter.isEmpty() && mi.type.compare(typeFilter, Qt::CaseInsensitive) != 0) continue;

        double score = 0.0;

        // === Name matching (always checked) ===
        if (!q.isEmpty()) {
            const QString lowerName = mi.name.toLower();
            if (lowerName.contains(q)) {
                score += 10000.0; // High base score for name match
                if (lowerName.startsWith(q)) {
                    score += 5000.0; // Bonus for prefix match
                }
                results.append({mi, score});
                continue; // Already matched by name, skip semantic keywords
            }
        }

        // === Semantic keyword matching ===
        // Always include the model, apply semantic score if keyword matches
        if (!q.isEmpty()) {
            applyKeywordScore(q, mi, score);
        }

        results.append({mi, score});
    }

    // Sort by score descending, then by name alphabetically
    std::sort(results.begin(), results.end(), [](const ScoredModel& a, const ScoredModel& b) {
        if (std::abs(a.score - b.score) > 0.01) return a.score > b.score;
        return a.info.name.compare(b.info.name, Qt::CaseInsensitive) < 0;
    });

    return results;
}

double SpiceModelSearch::parseParam(const SpiceModelInfo& info, const QString& key, double defaultValue) {
    for (const QString& param : info.params) {
        const QStringList parts = param.split('=', Qt::SkipEmptyParts);
        if (parts.size() >= 2 && parts[0].trimmed().compare(key, Qt::CaseInsensitive) == 0) {
            return toScientific(parts[1].trimmed());
        }
    }
    return defaultValue;
}

double SpiceModelSearch::toScientific(const QString& str) {
    QString s = str.trimmed();
    if (s.isEmpty()) return 0.0;

    bool ok = false;
    double val = s.toDouble(&ok);
    if (ok) return val;

    QRegularExpression re("^([+-]?\\d*\\.?\\d+)([a-zA-Zµ]*)$");
    QRegularExpressionMatch match = re.match(s);
    if (match.hasMatch()) {
        double num = match.captured(1).toDouble(&ok);
        if (!ok) return 0.0;
        QString suffix = match.captured(2).toLower();

        double multiplier = 1.0;
        if (suffix == "p") multiplier = 1e-12;
        else if (suffix == "n") multiplier = 1e-9;
        else if (suffix == "u" || suffix == "µ") multiplier = 1e-6;
        else if (suffix == "m") multiplier = 1e-3;
        else if (suffix == "k") multiplier = 1e3;
        else if (suffix == "meg") multiplier = 1e6;
        else if (suffix == "g") multiplier = 1e9;
        else if (suffix == "t") multiplier = 1e12;

        return num * multiplier;
    }
    return 0.0;
}

void SpiceModelSearch::applyKeywordScore(const QString& keyword, const SpiceModelInfo& info, double& score) {
    const QString type = info.type.toUpper();

    // === POWER / HIGH CURRENT ===
    if (keyword == "power" || keyword == "high current" || keyword == "highcurrent") {
        if (type == "NMOS" || type == "PMOS") {
            score += 100.0; // Base score for MOS
            double kp = parseParam(info, "Kp");
            if (kp > 0) score += std::log10(kp) * 500.0 + 100.0;
            double rd = parseParam(info, "Rd");
            if (rd > 0 && rd < 1.0) score += 100.0;
        } else if (type == "NPN" || type == "PNP") {
            score += 100.0; // Base score for BJT
            double bf = parseParam(info, "Bf");
            if (bf > 50) score += bf * 2.0;
            double is = parseParam(info, "Is");
            if (is > 1e-12) score += 50.0;
        } else if (type == "DIODE") {
            score += 100.0; // Base score for diode
            double is = parseParam(info, "Is");
            if (is > 1e-9) score += std::log10(is) * 100.0 + 100.0;
            double bv = parseParam(info, "BV");
            if (bv > 50) score += 80.0;
        }
        return;
    }

    // === FAST / HIGH SPEED / RF ===
    if (keyword == "fast" || keyword == "speed" || keyword == "high speed" ||
        keyword == "highspeed" || keyword == "rf" || keyword == "high freq" || keyword == "highfreq") {
        if (type == "NMOS" || type == "PMOS") {
            score += 80.0;
            double cgd = parseParam(info, "Cgd");
            if (cgd > 0 && cgd < 1e-11) score += 200.0 - std::log10(cgd) * 100.0;
            double cgs = parseParam(info, "Cgs");
            if (cgs > 0 && cgs < 1e-11) score += 100.0;
        } else if (type == "NPN" || type == "PNP") {
            score += 80.0;
            double tf = parseParam(info, "Tf");
            if (tf > 0 && tf < 1e-8) score += 300.0 - std::log10(tf) * 100.0;
        } else if (type == "DIODE") {
            score += 80.0;
            double tt = parseParam(info, "tt");
            if (tt > 0 && tt < 1e-6) score += 200.0 - std::log10(tt) * 80.0;
            double cjo = parseParam(info, "Cjo");
            if (cjo > 0 && cjo < 1e-10) score += 50.0;
        }
        return;
    }

    // === LOW NOISE ===
    if (keyword == "low noise" || keyword == "lownoise" || keyword == "noise") {
        if (type == "NPN" || type == "PNP") {
            score += 80.0;
            double is = parseParam(info, "Is");
            if (is > 0 && is < 1e-14) score += 200.0;
        } else if (type == "DIODE") {
            score += 80.0;
            double is = parseParam(info, "Is");
            if (is > 0 && is < 1e-12) score += 100.0;
        }
        return;
    }

    // === LOW VOLTAGE / LOW VTO / LOGIC LEVEL ===
    if (keyword == "low voltage" || keyword == "lowvoltage" || keyword == "low vto" || keyword == "logic") {
        if (type == "NMOS" || type == "PMOS") {
            score += 80.0;
            double vto = parseParam(info, "Vto");
            if (vto != 0 && std::abs(vto) < 2.5) score += 200.0 - std::abs(vto) * 50.0;
        }
        return;
    }

    // === HIGH VOLTAGE ===
    if (keyword == "high voltage" || keyword == "highvoltage" || keyword == "hv") {
        if (type == "NMOS" || type == "PMOS") {
            score += 80.0;
            double vto = parseParam(info, "Vto");
            if (std::abs(vto) > 3.0) score += std::abs(vto) * 30.0;
        } else if (type == "DIODE") {
            score += 80.0;
            double bv = parseParam(info, "BV");
            if (bv > 50) score += bv * 0.5;
        }
        return;
    }

    // === ZENER ===
    if (keyword == "zener") {
        if (type == "DIODE") {
            score += 500.0;
            double bv = parseParam(info, "BV");
            if (bv > 0) score += bv * 5.0;
        }
        return;
    }

    // === SCHOTTKY ===
    if (keyword == "schottky") {
        if (type == "DIODE") {
            score += 500.0;
            double is = parseParam(info, "Is");
            if (is > 1e-12) score += 100.0;
        }
        return;
    }

    // === SMALL SIGNAL ===
    if (keyword == "small signal" || keyword == "smallsignal" || keyword == "signal") {
        if (type == "NPN" || type == "PNP") {
            score += 80.0;
            double bf = parseParam(info, "Bf");
            if (bf > 0 && bf < 400) score += 100.0;
        }
        return;
    }

    // === SWITCHING ===
    if (keyword == "switching" || keyword == "switch") {
        if (type == "NPN" || type == "PNP") {
            score += 80.0;
            double tf = parseParam(info, "Tf");
            if (tf > 0 && tf < 5e-9) score += 150.0;
        } else if (type == "DIODE") {
            score += 80.0;
            double tt = parseParam(info, "tt");
            if (tt > 0 && tt < 1e-7) score += 150.0;
        } else if (type == "NMOS" || type == "PMOS") {
            score += 80.0;
            double cgd = parseParam(info, "Cgd");
            if (cgd > 0 && cgd < 1e-11) score += 150.0;
        }
        return;
    }

    // === GENERAL PURPOSE ===
    if (keyword == "general" || keyword == "general purpose" || keyword == "generalpurpose") {
        score += 50.0;
        return;
    }

    // Unknown keyword: no score change (all models stay visible)
}
