/*
 * Copyright 2026 Janada Sroor
 * SPDX-License-Identifier: Apache-2.0
 */

#include "spice_compat_rewriter.h"
#include "../../../simulator/core/sim_value_parser.h"
#include "model_injector.h"
#include "xspice_block_translator.h"
#include <QRegularExpression>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <cmath>
#include <limits>

namespace {

struct PwlPoint {
    double time = 0.0;
    QString timeText;
    QString value;
};

QString stripOuterBraces(const QString& text) {
    const QString trimmed = text.trimmed();
    if (trimmed.size() >= 2 && trimmed.startsWith('{') && trimmed.endsWith('}')) {
        return trimmed.mid(1, trimmed.size() - 2).trimmed();
    }
    return trimmed;
}

QString formatPwlNumber(double value) {
    return QString::number(value, 'g', 12);
}

QString scaledPwlExpr(const QString& expr, double scale) {
    if (qFuzzyCompare(scale, 1.0)) return QString("(%1)").arg(expr);
    return QString("((%1)*%2)").arg(expr, formatPwlNumber(scale));
}

QString formatPwlTimeText(const PwlPoint& point) {
    return point.timeText.isEmpty() ? formatPwlNumber(point.time) : point.timeText;
}

bool pwlValuesEquivalent(const QString& lhs, const QString& rhs) {
    double lhsNum = 0.0;
    double rhsNum = 0.0;
    if (SimValueParser::parseSpiceNumber(lhs, lhsNum) && SimValueParser::parseSpiceNumber(rhs, rhsNum)) {
        return qFuzzyCompare(1.0 + lhsNum, 1.0 + rhsNum);
    }
    return lhs.trimmed().compare(rhs.trimmed(), Qt::CaseInsensitive) == 0;
}

QString stripOuterQuotes(const QString& text) {
    if (text.size() >= 2 && ((text.startsWith('"') && text.endsWith('"')) || (text.startsWith('\'') && text.endsWith('\'')))) {
        return text.mid(1, text.size() - 2);
    }
    return text;
}

QStringList tokenizePwlBody(const QString& text) {
    QStringList tokens;
    QString current;
    int parenDepth = 0;
    int braceDepth = 0;
    bool inQuotes = false;
    QChar quoteChar;

    for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        if (inQuotes) {
            current += ch;
            if (ch == quoteChar) inQuotes = false;
            continue;
        }
        if (ch == '\'' || ch == '"') {
            inQuotes = true;
            quoteChar = ch;
            current += ch;
            continue;
        }
        if (ch == '(') ++parenDepth;
        else if (ch == ')' && parenDepth > 0) --parenDepth;
        else if (ch == '{') ++braceDepth;
        else if (ch == '}' && braceDepth > 0) --braceDepth;

        if ((ch.isSpace() || ch == ',') && parenDepth == 0 && braceDepth == 0) {
            if (!current.trimmed().isEmpty()) tokens.append(current.trimmed());
            current.clear();
            continue;
        }
        current += ch;
    }
    if (!current.trimmed().isEmpty()) tokens.append(current.trimmed());
    return tokens;
}

bool loadPwlPointsFromFile(const QString& fileToken, const QString& projectDir, bool scopeData,
                           QList<PwlPoint>* points, QString* error) {
    if (!points) return false;
    QString path = stripOuterQuotes(fileToken.trimmed());
    QFileInfo fi(path);
    if (fi.isRelative() && !projectDir.isEmpty()) path = QDir(projectDir).filePath(path);

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = QString("Could not open LT PWL file '%1'.").arg(fileToken);
        return false;
    }

    QString allText;
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (!scopeData) {
            const int semicolon = line.indexOf(';');
            const int hash = line.indexOf('#');
            const int star = line.trimmed().startsWith('*') ? 0 : -1;
            int commentPos = -1;
            if (semicolon >= 0) commentPos = semicolon;
            if (hash >= 0 && (commentPos < 0 || hash < commentPos)) commentPos = hash;
            if (star == 0) commentPos = 0;
            if (commentPos == 0) continue;
            if (commentPos > 0) line = line.left(commentPos);
        }
        allText += line;
        allText += ' ';
    }

    const QStringList tokens = tokenizePwlBody(allText);
    if (tokens.size() < 2 || (tokens.size() % 2) != 0) {
        if (error) *error = QString("LT PWL file '%1' does not contain time/value pairs.").arg(fileToken);
        return false;
    }

    QMap<double, QList<double>> scopedBuckets;
    for (int i = 0; i < tokens.size(); i += 2) {
        double timeValue = 0.0;
        double yValue = 0.0;
        if (!SimValueParser::parseSpiceNumber(tokens.at(i), timeValue) || !SimValueParser::parseSpiceNumber(tokens.at(i + 1), yValue)) {
            if (error) *error = QString("LT PWL file '%1' contains non-numeric data unsupported by VioSpice.").arg(fileToken);
            return false;
        }
        if (scopeData) {
            if (timeValue < 0.0) continue;
            scopedBuckets[timeValue].append(yValue);
        } else {
            points->append({timeValue, formatPwlNumber(timeValue), tokens.at(i + 1)});
        }
    }

    if (scopeData) {
        for (auto it = scopedBuckets.cbegin(); it != scopedBuckets.cend(); ++it) {
            double avg = 0.0;
            for (double v : it.value()) avg += v;
            avg /= static_cast<double>(it.value().size());
            points->append({it.key(), formatPwlNumber(it.key()), formatPwlNumber(avg)});
        }
    }

    return !points->isEmpty();
}

bool loadCurrentTablePairsFromFile(const QString& fileToken, const QString& projectDir, QStringList* pairs, QString* error) {
    if (!pairs) return false;
    QString path = stripOuterQuotes(fileToken.trimmed());
    QFileInfo fi(path);
    if (fi.isRelative() && !projectDir.isEmpty()) path = QDir(projectDir).filePath(path);

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = QString("Could not open LT current-source table file '%1'.").arg(fileToken);
        return false;
    }

    QString allText;
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        const int semicolon = line.indexOf(';');
        const int hash = line.indexOf('#');
        const int star = line.trimmed().startsWith('*') ? 0 : -1;
        int commentPos = -1;
        if (semicolon >= 0) commentPos = semicolon;
        if (hash >= 0 && (commentPos < 0 || hash < commentPos)) commentPos = hash;
        if (star == 0) commentPos = 0;
        if (commentPos == 0) continue;
        if (commentPos > 0) line = line.left(commentPos);
        allText += line;
        allText += ' ';
    }

    *pairs = tokenizePwlBody(allText);
    if (pairs->size() < 2 || (pairs->size() % 2) != 0) {
        if (error) *error = QString("LT current-source table file '%1' does not contain voltage/current pairs.").arg(fileToken);
        return false;
    }
    return true;
}

QString buildStepApproxPwl(const QString& initialValue, const QStringList& stepValues) {
    QStringList tokens;
    const QString settleTime = "1m";
    const QString rampTime = "10u";

    QString currentTime = "0";
    QString activeValue = initialValue.trimmed();
    tokens << currentTime << activeValue;
    for (const QString& nextValue : stepValues) {
        tokens << QString("{%1}").arg(currentTime.isEmpty() ? settleTime : currentTime + "+" + settleTime) << activeValue;
        currentTime = QString("{%1+%2+%3}").arg(currentTime, settleTime, rampTime);
        activeValue = nextValue.trimmed();
        tokens << currentTime << activeValue;
    }
    return QString("PWL(%1)").arg(tokens.join(' '));
}

bool appendPwlPointList(const QString& listText, double timeScale, double valueScale, double baseTime, QList<PwlPoint>* points,
                        QString* error) {
    if (!points) return false;
    const QStringList items = SpiceCompatRewriter::splitTopLevelSpiceArgs(listText);
    if (items.size() < 2 || (items.size() % 2) != 0) {
        if (error) *error = "LT PWL point list must contain time/value pairs.";
        return false;
    }

    double previousTime = points->isEmpty() ? baseTime : points->last().time;
    QString previousTimeText = points->isEmpty() ? formatPwlNumber(baseTime) : formatPwlTimeText(points->last());
    for (int i = 0; i < items.size(); i += 2) {
        const QString timeToken = items.at(i).trimmed();
        const QString valueToken = items.at(i + 1).trimmed();
        double parsedTime = 0.0;
        double parsedValue = 0.0;
        const QString normalizedTime = timeToken.startsWith('+') ? timeToken.mid(1).trimmed() : timeToken;
        const bool timeIsNumeric = SimValueParser::parseSpiceNumber(normalizedTime, parsedTime);
        if (!SimValueParser::parseSpiceNumber(valueToken, parsedValue)) {
            if (error) *error = QString("Unsupported LT PWL expression '%1, %2'; VioSpice currently requires numeric values.").arg(timeToken, valueToken);
            return false;
        }

        if (timeIsNumeric) {
            double finalTime = timeToken.startsWith('+') ? previousTime + (parsedTime * timeScale) : baseTime + (parsedTime * timeScale);
            previousTime = finalTime;
            previousTimeText = formatPwlNumber(finalTime);
            points->append({finalTime, previousTimeText, formatPwlNumber(parsedValue * valueScale)});
            continue;
        }

        if (!(normalizedTime.startsWith('{') && normalizedTime.endsWith('}'))) {
            if (error) *error = QString("Unsupported LT PWL time expression '%1'; VioSpice currently supports numeric times or brace expressions.").arg(timeToken);
            return false;
        }

        const QString expr = stripOuterBraces(normalizedTime);
        QString finalTimeText;
        if (timeToken.startsWith('+')) {
            finalTimeText = QString("{%1+%2}").arg(previousTimeText, scaledPwlExpr(expr, timeScale));
        } else {
            const QString scaledExpr = scaledPwlExpr(expr, timeScale);
            finalTimeText = qFuzzyIsNull(baseTime) ? QString("{%1}").arg(scaledExpr)
                                                  : QString("{%1+%2}").arg(formatPwlNumber(baseTime), scaledExpr);
        }
        previousTime = std::numeric_limits<double>::quiet_NaN();
        previousTimeText = finalTimeText;
        points->append({previousTime, finalTimeText, formatPwlNumber(parsedValue * valueScale)});
    }
    return true;
}

bool appendExpandedPwlSpecs(const QStringList& tokens, int* index, double timeScale, double valueScale, double baseTime,
                            QList<PwlPoint>* points, QStringList* warnings, const QString& projectDir, QString* error) {
    if (!index || !points) return false;

    auto isPwlKeyword = [](const QString& text) {
        return text.compare("ENDREPEAT", Qt::CaseInsensitive) == 0 ||
               text.compare("REPEAT", Qt::CaseInsensitive) == 0 ||
               text.compare("FOR", Qt::CaseInsensitive) == 0 ||
               text.compare("FOREVER", Qt::CaseInsensitive) == 0 ||
               text.startsWith("FILE=", Qt::CaseInsensitive) ||
               text.startsWith("SCOPEDATA=", Qt::CaseInsensitive);
    };

    while (*index < tokens.size()) {
        const QString token = tokens.at(*index);
        if (token.compare("ENDREPEAT", Qt::CaseInsensitive) == 0) {
            ++(*index);
            return true;
        }
        if (token.startsWith("(") && token.endsWith(")")) {
            if (!appendPwlPointList(token.mid(1, token.size() - 2), timeScale, valueScale, baseTime, points, error)) return false;
            ++(*index);
            continue;
        }
        if (token.startsWith("FILE=", Qt::CaseInsensitive) || token.startsWith("SCOPEDATA=", Qt::CaseInsensitive)) {
            QList<PwlPoint> filePoints;
            const bool scopeData = token.startsWith("SCOPEDATA=", Qt::CaseInsensitive);
            const QString fileToken = token.mid(scopeData ? 10 : 5);
            if (!loadPwlPointsFromFile(fileToken, projectDir, scopeData, &filePoints, error)) return false;
            if (filePoints.isEmpty()) {
                if (error) *error = QString("LT PWL %1 file '%2' contained no usable points.").arg(scopeData ? "SCOPEDATA" : "FILE", fileToken);
                return false;
            }
            const double origin = points->isEmpty() ? baseTime : points->last().time;
            for (const PwlPoint& point : filePoints) {
                double scaledValue = 0.0;
                if (!SimValueParser::parseSpiceNumber(point.value, scaledValue)) {
                    if (error) *error = QString("LT PWL file '%1' contains unsupported non-numeric values.").arg(fileToken);
                    return false;
                }
                const double finalTime = origin + (point.time * timeScale);
                points->append({finalTime, formatPwlNumber(finalTime), formatPwlNumber(scaledValue * valueScale)});
            }
            ++(*index);
            continue;
        }
        if (token.compare("REPEAT", Qt::CaseInsensitive) == 0) {
            ++(*index);
            if (*index >= tokens.size()) {
                if (error) *error = "Incomplete LT PWL REPEAT block.";
                return false;
            }
            bool repeatForever = false;
            int repeatCount = 0;
            if (tokens.at(*index).compare("FOREVER", Qt::CaseInsensitive) == 0) {
                repeatForever = true;
                ++(*index);
            } else {
                if (tokens.at(*index).compare("FOR", Qt::CaseInsensitive) == 0) ++(*index);
                if (*index >= tokens.size()) {
                    if (error) *error = "Incomplete LT PWL REPEAT FOR count.";
                    return false;
                }
                double parsedCount = 0.0;
                if (!SimValueParser::parseSpiceNumber(tokens.at(*index), parsedCount) || parsedCount < 0.0) {
                    if (error) *error = QString("Unsupported LT PWL repeat count '%1'.").arg(tokens.at(*index));
                    return false;
                }
                repeatCount = static_cast<int>(std::llround(parsedCount));
                ++(*index);
            }

            QList<PwlPoint> repeatedBody;
            int nestedIndex = *index;
            if (!appendExpandedPwlSpecs(tokens, &nestedIndex, timeScale, valueScale, 0.0, &repeatedBody, warnings, projectDir, error)) return false;
            *index = nestedIndex;
            if (repeatedBody.isEmpty()) continue;

            if (repeatForever) {
                if (warnings) warnings->append("LT PWL REPEAT FOREVER is not fully supported by VioSpice; keeping a single waveform period.");
                repeatCount = 1;
            }
            if (repeatCount > 1 && !repeatedBody.isEmpty() && qFuzzyIsNull(repeatedBody.first().time) &&
                !pwlValuesEquivalent(repeatedBody.first().value, repeatedBody.last().value)) {
                if (error) *error = "Ill-formed LT PWL REPEAT block: first repeated time is zero but first and last values differ.";
                return false;
            }
            const double span = repeatedBody.last().time;
            for (int rep = 0; rep < repeatCount; ++rep) {
                const double origin = points->isEmpty() ? baseTime : points->last().time;
                for (const PwlPoint& point : repeatedBody) {
                    points->append({origin + point.time, formatPwlNumber(origin + point.time), point.value});
                }
                if ((!std::isfinite(span) || span <= 0.0) && rep + 1 < repeatCount) {
                    if (error) *error = "Ill-formed LT PWL REPEAT block with zero span.";
                    return false;
                }
            }
            continue;
        }

        if (!isPwlKeyword(token)) {
            QStringList pointTokens;
            int pointIndex = *index;
            while (pointIndex < tokens.size() && !isPwlKeyword(tokens.at(pointIndex))) {
                pointTokens.append(tokens.at(pointIndex));
                ++pointIndex;
            }
            if (!appendPwlPointList(pointTokens.join(','), timeScale, valueScale, baseTime, points, error)) return false;
            *index = pointIndex;
            continue;
        }

        if (error) *error = QString("Unsupported LT PWL token '%1'.").arg(token);
        return false;
    }
    return true;
}

} // namespace

bool SpiceCompatRewriter::rewriteLtCurrentSourceSpecial(const QString& ref, const QString& nplus, const QString& nminus,
                                                        const QString& value, const QString& projectDir,
                                                        QString* replacement, QStringList* warnings) {
    if (!replacement) return false;
    const QString trimmed = value.trimmed();

    {
        static const QRegularExpression resistiveRe(R"(^R\s*=\s*(.+)$)", QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = resistiveRe.match(trimmed);
        if (m.hasMatch()) {
            const QString rval = m.captured(1).trimmed();
            *replacement = QString("R__ILOAD_%1 %2 %3 %4").arg(ref, nplus, nminus, rval);
            if (warnings) warnings->append(QString("Rewrote LT current-source R= load on %1 into an equivalent resistor for ngspice.").arg(ref));
            return true;
        }
    }

    {
        static const QRegularExpression tableRe(R"(^(?:tbl|table)\s*=\s*(.+)$)", QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = tableRe.match(trimmed);
        if (m.hasMatch()) {
            QString spec = m.captured(1).trimmed();
            QStringList args;
            if (spec.startsWith('(') && spec.endsWith(')')) {
                args = SpiceCompatRewriter::splitTopLevelSpiceArgs(spec.mid(1, spec.size() - 2));
            } else if ((spec.startsWith('"') && spec.endsWith('"')) || (!spec.contains(',') && !spec.contains('(') && !spec.contains('{'))) {
                QString error;
                if (!loadCurrentTablePairsFromFile(spec, projectDir, &args, &error)) {
                    if (warnings && !error.isEmpty()) warnings->append(error);
                    return false;
                }
            } else if (spec.startsWith('{') && spec.endsWith('}')) {
                if (warnings) warnings->append(QString("LT current-source table filename parameter on %1 is not yet supported by VioSpice.").arg(ref));
                return false;
            }

            QString error;
            const QString expr = SpiceCompatRewriter::buildCurrentTableExpr(QString("V(%1,%2)").arg(nplus, nminus), args, &error);
            if (expr.isEmpty()) {
                if (warnings && !error.isEmpty()) warnings->append(error);
                return false;
            }
            QString bref = ref;
            if (!bref.startsWith('B', Qt::CaseInsensitive)) bref = "B__ITBL_" + ref;
            *replacement = QString("%1 %2 %3 I={%4}").arg(bref, nplus, nminus, expr);
            if (warnings) warnings->append(QString("Rewrote LT current-source tbl/table on %1 into a behavioral current source for ngspice.").arg(ref));
            return true;
        }
    }

    {
        static const QRegularExpression stepRe(R"(^(.+?)\s+step\s*\((.*)\)\s*(load)?\s*$)", QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = stepRe.match(trimmed);
        if (m.hasMatch()) {
            const QString baseValue = m.captured(1).trimmed();
            const QStringList stepValues = SpiceCompatRewriter::splitTopLevelSpiceArgs(m.captured(2));
            if (!stepValues.isEmpty()) {
                *replacement = QString("%1 %2 %3 %4").arg(ref, nplus, nminus, buildStepApproxPwl(baseValue, stepValues));
                if (warnings) warnings->append(QString("Approximated LT current-source step(...) on %1 with a heuristic PWL load sequence; LT steady-state step timing is not fully reproduced.").arg(ref));
                return true;
            }
        }
    }

    return false;
}

QString SpiceCompatRewriter::inlinePwlFileIfNeeded(const QString& value, const QString& projectDir, QStringList* warnings) {
    const QString v = value.trimmed();
    if (!v.contains("PWL", Qt::CaseInsensitive)) return value;

    QString body;
    QString tail;

    // Regex to match PWL(...) and capture the inside, also allowing trailing params
    QRegularExpression re(R"(^PWL\s*\((.*)\)\s*(.*)$)", QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch fullMatch = re.match(v);
    if (fullMatch.hasMatch()) {
        body = fullMatch.captured(1).trimmed();
        tail = fullMatch.captured(2).trimmed();
    } else {
        // Try without parentheses: PWL 0 0 1 1 ...
        QRegularExpression re2(R"(^PWL\s+(.*)$)", QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
        const QRegularExpressionMatch match2 = re2.match(v);
        if (!match2.hasMatch()) return value;
        body = match2.captured(1).trimmed();
    }

    // Check for FILE="..." syntax. If no scaling factors are present and it's a simple file-backed PWL, 
    // use native pwlfile for efficiency. Otherwise, expansion below handles scaling/nesting.
    if (!body.contains("SCALE_FACTOR", Qt::CaseInsensitive)) {
        QRegularExpression fileRe(R"(FILE\s*=\s*["']?([^"']+)["']?)", QRegularExpression::CaseInsensitiveOption);
        auto fileMatch = fileRe.match(body);
        if (fileMatch.hasMatch()) {
            QString path = fileMatch.captured(1);
            // Use the new native VioMATRIXC 'pwlfile' parameter
            QString result = QString("pwlfile=\"%1\"").arg(path);
            if (!tail.isEmpty()) result += " " + tail;
            return result;
        }
    }

    QStringList tokens = tokenizePwlBody(body);
    if (tokens.isEmpty()) return value;

    double timeScale = 1.0;
    double valueScale = 1.0;
    while (!tokens.isEmpty()) {
        const QString token = tokens.first();
        if (token.startsWith("TIME_SCALE_FACTOR=", Qt::CaseInsensitive)) {
            if (!SimValueParser::parseSpiceNumber(token.mid(18), timeScale)) return value;
            tokens.removeFirst();
            continue;
        }
        if (token.startsWith("VALUE_SCALE_FACTOR=", Qt::CaseInsensitive)) {
            if (!SimValueParser::parseSpiceNumber(token.mid(19), valueScale)) return value;
            tokens.removeFirst();
            continue;
        }
        break;
    }

    QList<PwlPoint> points;
    int index = 0;
    QString error;
    if (!appendExpandedPwlSpecs(tokens, &index, timeScale, valueScale, 0.0, &points, warnings, projectDir, &error) || index != tokens.size()) {
        if (warnings && !error.isEmpty()) warnings->append(QString("Could not fully translate LT PWL syntax '%1': %2").arg(v, error));
        return value;
    }
    if (points.isEmpty()) return value;

    QStringList flatTokens;
    for (const PwlPoint& point : points) {
        flatTokens << formatPwlTimeText(point) << point.value;
    }
    
    QString result = QString("PWL(%1)").arg(flatTokens.join(' '));
    if (!tail.isEmpty()) result += " " + tail;
    return result;
}

bool SpiceCompatRewriter::convertLtConditionToStepExpr(const QString& condition, QString* stepExpr) {
    if (!stepExpr) return false;

    int parenDepth = 0;
    int braceDepth = 0;
    int opPos = -1;
    QString op;
    for (int i = 0; i < condition.size(); ++i) {
        const QChar ch = condition.at(i);
        if (ch == '(') ++parenDepth;
        else if (ch == ')' && parenDepth > 0) --parenDepth;
        else if (ch == '{') ++braceDepth;
        else if (ch == '}' && braceDepth > 0) --braceDepth;
        if (parenDepth != 0 || braceDepth != 0) continue;
        if (i + 1 < condition.size()) {
            const QString two = condition.mid(i, 2);
            if (two == ">=" || two == "<=") {
                opPos = i;
                op = two;
                break;
            }
        }
        if (ch == '>' || ch == '<') {
            opPos = i;
            op = ch;
            break;
        }
    }

    if (opPos >= 0) {
        const QString lhs = condition.left(opPos).trimmed();
        const QString rhs = condition.mid(opPos + op.size()).trimmed();
        if (lhs.isEmpty() || rhs.isEmpty()) return false;
        if (op == ">" || op == ">=") *stepExpr = QString("u((%1)-(%2))").arg(lhs, rhs);
        else if (op == "<" || op == "<=") *stepExpr = QString("u((%1)-(%2))").arg(rhs, lhs);
        else return false;
        return true;
    }

    *stepExpr = QString("u((%1)-(0.5))").arg(condition.trimmed());
    return !condition.trimmed().isEmpty();
}

void SpiceCompatRewriter::updateSubcktDepthForLine(const QString& line, int& subcktDepth) {
    const QString trimmed = line.trimmed();
    if (!trimmed.startsWith('.')) return;
    const QString card = trimmed.section(QRegularExpression("\\s+"), 0, 0).trimmed().toLower();
    if (card == ".subckt") {
        ++subcktDepth;
    } else if (card == ".ends" && subcktDepth > 0) {
        --subcktDepth;
    }
}

QString SpiceCompatRewriter::rewriteLtBehavioralIf(const QString& line, QStringList* warnings) {
    QString out = line;

    auto findTopLevelComparison = [](const QString& text, int* opPos, QString* op) {
        int parenDepth = 0;
        int braceDepth = 0;
        for (int i = 0; i < text.size(); ++i) {
            const QChar ch = text.at(i);
            if (ch == '(') ++parenDepth;
            else if (ch == ')' && parenDepth > 0) --parenDepth;
            else if (ch == '{') ++braceDepth;
            else if (ch == '}' && braceDepth > 0) --braceDepth;
            if (parenDepth != 0 || braceDepth != 0) continue;
            if (i + 1 < text.size()) {
                const QString two = text.mid(i, 2);
                if (two == ">=" || two == "<=") {
                    *opPos = i;
                    *op = two;
                    return true;
                }
            }
            if (ch == '>' || ch == '<') {
                *opPos = i;
                *op = ch;
                return true;
            }
        }
        return false;
    };

    bool changed = false;
    bool rewroteNonZeroFalseBranch = false;
    while (true) {
        const int ifPos = out.indexOf(QRegularExpression("\\bif\\s*\\(", QRegularExpression::CaseInsensitiveOption));
        if (ifPos < 0) break;
        const int openPos = out.indexOf('(', ifPos);
        const int closePos = findMatchingParen(out, openPos);
        if (openPos < 0 || closePos < 0) break;

        const QString inside = out.mid(openPos + 1, closePos - openPos - 1);
        const QStringList args = splitTopLevelSpiceArgs(inside);
        if (args.size() != 3) break;

        const QString condExpr = args.at(0).trimmed();
        const QString trueExpr = args.at(1).trimmed();
        const QString falseExpr = args.at(2).trimmed();

        int opPos = -1;
        QString op;
        if (!findTopLevelComparison(condExpr, &opPos, &op)) break;

        const QString lhs = condExpr.left(opPos).trimmed();
        const QString rhs = condExpr.mid(opPos + op.size()).trimmed();
        if (lhs.isEmpty() || rhs.isEmpty()) break;

        QString stepExpr;
        if (op == ">" || op == ">=") stepExpr = QString("u((%1)-(%2))").arg(lhs, rhs);
        else if (op == "<" || op == "<=") stepExpr = QString("u((%1)-(%2))").arg(rhs, lhs);
        else break;

        const bool falseIsZero = falseExpr == "0" || falseExpr == "0.0";
        QString replacement;
        if (falseIsZero) {
            replacement = QString("((%1)*(%2))").arg(trueExpr, stepExpr);
        } else {
            replacement = QString("((%1)*(%2) + (%3)*(1-(%2)))").arg(trueExpr, stepExpr, falseExpr);
            rewroteNonZeroFalseBranch = true;
        }

        out.replace(ifPos, closePos - ifPos + 1, replacement);
        changed = true;
    }

    if (changed && warnings) {
        warnings->append(QString("Rewrote LT-style if(...) to ngspice-safe expression in: %1").arg(line.trimmed()));
        if (rewroteNonZeroFalseBranch) {
            warnings->append(QString("Rewrote LT-style if(..., true, false) into weighted u(...) form in: %1").arg(line.trimmed()));
        }
    }
    return out;
}

QString SpiceCompatRewriter::rewriteLtVoltageSourceExtras(const QString& line, QStringList* warnings) {
    QString out = line;

    static const QRegularExpression voltageSourceExtrasRe(
        "^\\s*(V[^\\s]*)\\s+(\\S+)\\s+(\\S+)\\s+(.+?)\\s+(.*)$",
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch sourceMatch = voltageSourceExtrasRe.match(out);
    if (!sourceMatch.hasMatch()) return out;

    const QString ref = sourceMatch.captured(1).trimmed();
    const QString nodePlus = sourceMatch.captured(2).trimmed();
    const QString nodeMinus = sourceMatch.captured(3).trimmed();
    const QString value = sourceMatch.captured(4).trimmed();
    QString extras = sourceMatch.captured(5).trimmed();

    static const QRegularExpression rserRe("\\bRser\\s*=\\s*([^\\s]+)", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression cparRe("\\bCpar\\s*=\\s*([^\\s]+)", QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch rserMatch = rserRe.match(extras);
    const QRegularExpressionMatch cparMatch = cparRe.match(extras);

    if (!rserMatch.hasMatch() && !cparMatch.hasMatch()) return out;

    const QString rser = rserMatch.hasMatch() ? rserMatch.captured(1).trimmed() : QString();
    const QString cpar = cparMatch.hasMatch() ? cparMatch.captured(1).trimmed() : QString();

    extras.remove(rserRe);
    extras.remove(cparRe);
    extras = extras.simplified();

    const QString sourcePlusNode = rser.isEmpty() ? nodePlus : QString("%1__rser").arg(ref);
    QStringList rewrittenLines;

    QString sourceLine = QString("%1 %2 %3 %4").arg(ref, sourcePlusNode, nodeMinus, value);
    if (!extras.isEmpty()) sourceLine += " " + extras;
    rewrittenLines << sourceLine;

    if (!rser.isEmpty()) {
        rewrittenLines << QString("R__RSER_%1 %2 %3 %4").arg(ref, nodePlus, sourcePlusNode, rser);
    }
    if (!cpar.isEmpty()) {
        rewrittenLines << QString("C__CPAR_%1 %2 %3 %4").arg(ref, nodePlus, nodeMinus, cpar);
    }

    out = rewrittenLines.join("\n");
    if (warnings) {
        if (!rser.isEmpty() && !cpar.isEmpty()) {
            warnings->append(QString("Expanded LT voltage source Rser=/Cpar= on %1 into explicit series resistor and shunt capacitor for ngspice.").arg(ref));
        } else if (!rser.isEmpty()) {
            warnings->append(QString("Expanded LT voltage source Rser= on %1 into explicit series resistor for ngspice.").arg(ref));
        } else {
            warnings->append(QString("Expanded LT voltage source Cpar= on %1 into explicit shunt capacitor for ngspice.").arg(ref));
        }
    }
    return out;
}

QString SpiceCompatRewriter::rewriteWavefileSourceSyntax(const QString& line, QStringList* warnings) {
    // VioMATRIXC audio sources are instance parameters (`wavefile=`/`chan=`).
    // A hand-typed positional `WAVEFILE "path" CHAN n` is not mapped by the
    // deck parser and silently degrades to DC 0 (zero output), so normalize
    // it here. Already-param form (`wavefile=`) passes through untouched.
    static const QRegularExpression sourceRe(
        "^\\s*([VI]\\S*)\\s+(\\S+)\\s+(\\S+)\\s+WAVEFILE\\s+\"([^\"]+)\"\\s*(.*)$",
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = sourceRe.match(line);
    if (!m.hasMatch()) return line;

    const QString ref = m.captured(1).trimmed();
    const QString tail = m.captured(5).trimmed();
    // Bail if the tail already carries param syntax (e.g. wavefile= used as
    // a trailing param after some other value form).
    if (tail.contains("wavefile", Qt::CaseInsensitive)) return line;

    QString chan;
    static const QRegularExpression reChan(R"(CHAN\s*[= ]\s*(\d+))", QRegularExpression::CaseInsensitiveOption);
    const auto chanMatch = reChan.match(tail);
    if (chanMatch.hasMatch()) chan = chanMatch.captured(1);

    QString out = QString("%1 %2 %3 wavefile=\"%4\"")
        .arg(ref, m.captured(2).trimmed(), m.captured(3).trimmed(), m.captured(4).trimmed());
    if (!chan.isEmpty()) out += " chan=" + chan;
    if (warnings) {
        warnings->append(QString("Rewrote positional WAVEFILE source %1 to wavefile=/chan= param syntax for VioMATRIXC audio.").arg(ref));
    }
    return out;
}

QString SpiceCompatRewriter::rewriteLtTriggeredPulseSource(const QString& line, QStringList* warnings) {
    static const QRegularExpression sourceRe(
        "^\\s*(V\\S*)\\s+(\\S+)\\s+(\\S+)\\s+(.+)$",
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = sourceRe.match(line);
    if (!match.hasMatch()) return line;

    const QString ref = match.captured(1).trimmed();
    const QString nplus = match.captured(2).trimmed();
    const QString nminus = match.captured(3).trimmed();
    const QString rest = match.captured(4).trimmed();
    const int pulsePos = rest.indexOf(QRegularExpression("^PULSE\\s*\\(", QRegularExpression::CaseInsensitiveOption));
    if (pulsePos != 0) return line;
    const int openPos = rest.indexOf('(');
    const int closePos = findMatchingParen(rest, openPos);
    if (openPos < 0 || closePos < 0) return line;

    const QString pulseExpr = rest.left(closePos + 1).trimmed();
    QString tail = rest.mid(closePos + 1).trimmed();

    static const QRegularExpression triggerRe("\\bTrigger\\s*=\\s*(.+?)(?=\\s+tripd[vt]\\s*=|$)", QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch triggerMatch = triggerRe.match(tail);
    if (!triggerMatch.hasMatch()) return line;

    const QString triggerExpr = triggerMatch.captured(1).trimmed();
    QString stepExpr;
    if (!convertLtConditionToStepExpr(triggerExpr, &stepExpr)) return line;

    tail.remove(triggerRe);
    tail = tail.simplified();

    const QString hiddenNode = QString("%1__trigger_src").arg(ref);
    const QString hiddenRef = QString("V__TRIGSRC_%1").arg(ref);
    const QString bufferRef = QString("B__TRIGBUF_%1").arg(ref);

    QStringList rewrittenLines;
    QString hiddenLine = QString("%1 %2 %3 %4").arg(hiddenRef, hiddenNode, nminus, pulseExpr);
    if (!tail.isEmpty()) hiddenLine += " " + tail;
    rewrittenLines << hiddenLine;
    rewrittenLines << QString("%1 %2 %3 V={(%4)*V(%5,%6)}").arg(bufferRef, nplus, nminus, stepExpr, hiddenNode, nminus);

    if (warnings) {
        warnings->append(QString("Approximated LT PULSE Trigger= behavior on %1 by gating a hidden pulse source with the trigger expression.").arg(ref));
        warnings->append(QString("LT triggered source restart semantics are only partially emulated for %1; the pulse is gated by the trigger but not restarted on each trigger event.").arg(ref));
    }
    return rewrittenLines.join("\n");
}

QString SpiceCompatRewriter::rewriteLtTriggeredPwlSource(const QString& line, QStringList* warnings) {
    static const QRegularExpression sourceRe(
        "^\\s*(V\\S*)\\s+(\\S+)\\s+(\\S+)\\s+(.+)$",
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = sourceRe.match(line);
    if (!match.hasMatch()) return line;

    const QString ref = match.captured(1).trimmed();
    const QString nplus = match.captured(2).trimmed();
    const QString nminus = match.captured(3).trimmed();
    const QString rest = match.captured(4).trimmed();
    if (!rest.startsWith("PWL", Qt::CaseInsensitive)) return line;

    static const QRegularExpression triggerRe("\\bTrigger\\s*=\\s*(.+?)(?=\\s+tripd[vt]\\s*=|$)", QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch triggerMatch = triggerRe.match(rest);
    if (!triggerMatch.hasMatch()) return line;

    QString stepExpr;
    const QString triggerExpr = triggerMatch.captured(1).trimmed();
    if (!convertLtConditionToStepExpr(triggerExpr, &stepExpr)) return line;

    QString pwlExpr = rest;
    pwlExpr.remove(triggerRe);
    pwlExpr = pwlExpr.simplified();

    const QString hiddenNode = QString("%1__trigger_src").arg(ref);
    const QString hiddenRef = QString("V__TRIGSRC_%1").arg(ref);
    const QString bufferRef = QString("B__TRIGBUF_%1").arg(ref);

    QStringList rewrittenLines;
    rewrittenLines << QString("%1 %2 %3 %4").arg(hiddenRef, hiddenNode, nminus, pwlExpr);
    rewrittenLines << QString("%1 %2 %3 V={(%4)*V(%5,%6)}").arg(bufferRef, nplus, nminus, stepExpr, hiddenNode, nminus);

    if (warnings) {
        warnings->append(QString("Approximated LT PWL Trigger= behavior on %1 by gating a hidden PWL source with the trigger expression.").arg(ref));
        warnings->append(QString("LT triggered PWL restart semantics are only partially emulated for %1; the waveform is gated by the trigger but not restarted on each trigger event.").arg(ref));
    }
    return rewrittenLines.join("\n");
}

QString SpiceCompatRewriter::rewriteLtTriggeredWaveSource(const QString& line, const QString& kind, QStringList* warnings) {
    static const QRegularExpression sourceRe(
        "^\\s*(V\\S*)\\s+(\\S+)\\s+(\\S+)\\s+(.+)$",
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = sourceRe.match(line);
    if (!match.hasMatch()) return line;

    const QString ref = match.captured(1).trimmed();
    const QString nplus = match.captured(2).trimmed();
    const QString nminus = match.captured(3).trimmed();
    const QString rest = match.captured(4).trimmed();
    if (!rest.startsWith(kind, Qt::CaseInsensitive)) return line;

    static const QRegularExpression triggerRe("\\bTrigger\\s*=\\s*(.+?)(?=\\s+tripd[vt]\\s*=|$)", QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch triggerMatch = triggerRe.match(rest);
    if (!triggerMatch.hasMatch()) return line;

    QString stepExpr;
    const QString triggerExpr = triggerMatch.captured(1).trimmed();
    if (!convertLtConditionToStepExpr(triggerExpr, &stepExpr)) return line;

    QString sourceExpr = rest;
    sourceExpr.remove(triggerRe);
    sourceExpr = sourceExpr.simplified();

    const QString hiddenNode = QString("%1__trigger_src").arg(ref);
    const QString hiddenRef = QString("V__TRIGSRC_%1").arg(ref);
    const QString bufferRef = QString("B__TRIGBUF_%1").arg(ref);

    QStringList rewrittenLines;
    rewrittenLines << QString("%1 %2 %3 %4").arg(hiddenRef, hiddenNode, nminus, sourceExpr);
    rewrittenLines << QString("%1 %2 %3 V={(%4)*V(%5,%6)}").arg(bufferRef, nplus, nminus, stepExpr, hiddenNode, nminus);

    if (warnings) {
        warnings->append(QString("Approximated LT %1 Trigger= behavior on %2 by gating a hidden %1 source with the trigger expression.").arg(kind.toUpper(), ref));
        warnings->append(QString("LT triggered %1 restart semantics are only partially emulated for %2; the waveform is gated by the trigger but not restarted on each trigger event.").arg(kind.toUpper(), ref));
    }
    return rewrittenLines.join("\n");
}

QString SpiceCompatRewriter::rewriteLtBehavioralFunctions(const QString& line, QStringList* warnings) {
    struct RewriteRule {
        QString name;
        int minArgs;
        int maxArgs;
    };

    const QList<RewriteRule> rules = {
        {"buf", 1, 1},
        {"inv", 1, 1},
        {"uramp", 1, 1},
        {"limit", 3, 3},
        {"dnlim", 3, 3},
        {"uplim", 3, 3},
    };

    QString out = line;
    bool changed = false;

    auto buildReplacement = [](const QString& name, const QStringList& args) {
        if (name.compare("buf", Qt::CaseInsensitive) == 0) {
            return QString("u((%1)-(0.5))").arg(args.at(0));
        }
        if (name.compare("inv", Qt::CaseInsensitive) == 0) {
            return QString("(1-u((%1)-(0.5)))").arg(args.at(0));
        }
        if (name.compare("uramp", Qt::CaseInsensitive) == 0) {
            return QString("((%1)*u(%1))").arg(args.at(0));
        }
        if (name.compare("limit", Qt::CaseInsensitive) == 0) {
            return QString("min(max((%1),min((%2),(%3))),max((%2),(%3)))").arg(args.at(0), args.at(1), args.at(2));
        }
        if (name.compare("dnlim", Qt::CaseInsensitive) == 0) {
            return QString("max((%1),(%2))").arg(args.at(0), args.at(1));
        }
        if (name.compare("uplim", Qt::CaseInsensitive) == 0) {
            return QString("min((%1),(%2))").arg(args.at(0), args.at(1));
        }
        return QString();
    };

    bool replaced = true;
    while (replaced) {
        replaced = false;
        for (const RewriteRule& rule : rules) {
            const QString needle = rule.name + "(";
            const int nameIndex = out.indexOf(needle, 0, Qt::CaseInsensitive);
            if (nameIndex < 0) continue;

            const int openIndex = nameIndex + rule.name.size();
            const int closeIndex = findMatchingParen(out, openIndex);
            if (closeIndex < 0) continue;

            const QString inner = out.mid(openIndex + 1, closeIndex - openIndex - 1);
            const QStringList args = splitTopLevelSpiceArgs(inner);
            if (args.size() < rule.minArgs || args.size() > rule.maxArgs) continue;

            const QString replacement = buildReplacement(rule.name, args);
            if (replacement.isEmpty()) continue;

            out.replace(nameIndex, closeIndex - nameIndex + 1, replacement);
            changed = true;
            replaced = true;
            break;
        }
    }

    if (changed && warnings) {
        warnings->append(QString("Rewrote LT behavioral helper functions for ngspice compatibility in: %1").arg(line.trimmed()));
    }

    return out;
}

QStringList SpiceCompatRewriter::tokenizeLtOtaLine(const QString& line) {
    return line.simplified().split(' ', Qt::SkipEmptyParts);
}

QString SpiceCompatRewriter::buildNgspiceOtaTranslation(const QString& line) {
    const QStringList tokens = tokenizeLtOtaLine(line);
    if (tokens.size() < 10) return QString();
    if (tokens.at(9).compare("OTA", Qt::CaseInsensitive) != 0) return QString();
    if (!tokens.at(0).startsWith('A', Qt::CaseInsensitive)) return QString();

    const QString ref = tokens.at(0);
    const QString n1 = tokens.at(1);
    const QString n2 = tokens.at(2);
    const QString n3 = tokens.at(3);
    const QString n4 = tokens.at(4);
    const QString rail = tokens.at(6);
    const QString out = tokens.at(7);
    const QString gnd = tokens.at(8);

    QMap<QString, QString> params;
    QSet<QString> flags;
    for (int i = 10; i < tokens.size(); ++i) {
        const QString token = tokens.at(i).trimmed();
        if (token.isEmpty()) continue;
        const int eq = token.indexOf('=');
        if (eq >= 0) {
            QString key = token.left(eq).trimmed().toLower();
            QString value = token.mid(eq + 1).trimmed();
            if (value.isEmpty() && i + 1 < tokens.size()) {
                value = tokens.at(++i).trimmed();
            }
            if (!key.isEmpty()) params.insert(key, value);
        } else {
            flags.insert(token.toLower());
        }
    }

    const QString gm = params.value("g", "1u");
    const QString refExpr = params.value("ref", "0");
    const QString upper = params.contains("iout") ? params.value("iout")
                       : params.contains("isrc") ? params.value("isrc")
                       : QStringLiteral("10u");
    const QString lower = params.contains("isink") ? params.value("isink")
                       : QStringLiteral("-(%1)").arg(upper);
    const QString rout = params.value("rout").trimmed();
    const QString cout = params.value("cout").trimmed();
    const QString vhigh = params.value("vhigh").trimmed();
    const QString vlow = params.value("vlow").trimmed();
    const QString epsilon = params.value("epsilon", "1u").trimmed();

    const QString diffExpr = QString("(((V(%1,%2))+(V(%3,%4)))-(%5))")
        .arg(n1, n2, n3, n4, refExpr);
    const QString rawExpr = QString("((%1)*(%2))").arg(gm, diffExpr);

    QString currentExpr;
    if (flags.contains("linear")) {
        currentExpr = QString("min(max((%1),(%2)),(%3))").arg(rawExpr, lower, upper);
    } else {
        const QString posExpr = QString("(u(%1)*((%2)*tanh((%1)/(max(abs((%2)),1e-30)))))")
            .arg(rawExpr, upper);
        const QString negExpr = QString("(u(-(%1))*((abs((%2)))*tanh((-(%1))/(max(abs((%2)),1e-30)))))")
            .arg(rawExpr, lower);
        currentExpr = QString("((%1)-(%2))").arg(posExpr, negExpr);
    }

    if (!vhigh.isEmpty() || !vlow.isEmpty()) {
        const QString highExpr = vhigh.isEmpty() ? QStringLiteral("1e308")
                                                 : QString("((V(%1,%2))+(%3))").arg(rail, gnd, vhigh);
        const QString lowExpr = vlow.isEmpty() ? QStringLiteral("-1e308")
                                               : QString("((V(%1,%2))+(%3))").arg(gnd, gnd, vlow);
        const QString voutExpr = QString("V(%1,%2)").arg(out, gnd);
        const QString compliance = QString("(u((%1)-(%2)+(%3))*u((%2)-(%4)+(%3)))")
            .arg(highExpr, voutExpr, epsilon, lowExpr);
        currentExpr = QString("((%1)*(%2))").arg(currentExpr, compliance);
    }

    QStringList lines;
    lines << QString("* OTA_TRANSLATED %1").arg(ref);
    lines << QString("B__OTA_%1 %2 %3 I={%4}").arg(XSpiceBlockTranslator::sanitizeMixedModeToken(ref), out, gnd, currentExpr);
    if (!rout.isEmpty()) {
        lines << QString("R__OTA_%1 %2 %3 %4").arg(XSpiceBlockTranslator::sanitizeMixedModeToken(ref), out, gnd, rout);
    }
    if (!cout.isEmpty()) {
        lines << QString("C__OTA_%1 %2 %3 %4").arg(XSpiceBlockTranslator::sanitizeMixedModeToken(ref), out, gnd, cout);
    }
    return lines.join('\n');
}

QString SpiceCompatRewriter::rewriteUnsupportedLtBehavioralTimeFunctions(const QString& line, QStringList* warnings) {
    QString out = line;
    struct FuncSpec { QString name; int minArgs; int maxArgs; };
    const QList<FuncSpec> funcs = {
        {"absdelay", 2, 3},
        {"delay", 2, 3},
    };

    bool changed = false;
    bool replaced = true;
    while (replaced) {
        replaced = false;
        for (const FuncSpec& func : funcs) {
            const int nameIndex = out.indexOf(QRegularExpression(QString("\\b%1\\s*\\(").arg(QRegularExpression::escape(func.name)),
                                                                    QRegularExpression::CaseInsensitiveOption));
            if (nameIndex < 0) continue;
            const int openIndex = out.indexOf('(', nameIndex);
            const int closeIndex = findMatchingParen(out, openIndex);
            if (closeIndex < 0) continue;

            const QString inner = out.mid(openIndex + 1, closeIndex - openIndex - 1);
            const QStringList args = splitTopLevelSpiceArgs(inner);
            if (args.size() < func.minArgs || args.size() > func.maxArgs) continue;

            const QString passthroughExpr = QString("(%1)").arg(args.at(0).trimmed());
            out.replace(nameIndex, closeIndex - nameIndex + 1, passthroughExpr);
            changed = true;
            replaced = true;
            if (warnings) {
                warnings->append(QString("Approximated LT %1(...) by passing through its input expression because this ngspice configuration does not support %1(...). Original line: %2")
                                     .arg(func.name, line.trimmed()));
            }
            break;
        }
    }

    return out;
}

QString SpiceCompatRewriter::rewriteUnsupportedLtTableFunction(const QString& line, QStringList* warnings) {
    QString out = line;
    bool changed = false;

    while (true) {
        const int tableIndex = out.indexOf(QRegularExpression("\\btable\\s*\\(", QRegularExpression::CaseInsensitiveOption));
        if (tableIndex < 0) break;
        const int openIndex = out.indexOf('(', tableIndex);
        const int closeIndex = findMatchingParen(out, openIndex);
        if (openIndex < 0 || closeIndex < 0) break;

        const QString inner = out.mid(openIndex + 1, closeIndex - openIndex - 1);
        const QStringList args = splitTopLevelSpiceArgs(inner);
        if (args.size() < 3) break;

        const QString xExpr = args.at(0).trimmed();
        QString replacement;
        if ((args.size() - 1) % 2 == 0) {
            QString expr = args.last().trimmed();
            for (int i = args.size() - 3; i >= 1; i -= 2) {
                const QString xk = args.at(i).trimmed();
                const QString yk = args.at(i + 1).trimmed();
                expr = QString("if((%1)<=(%2),(%3),(%4))").arg(xExpr, xk, yk, expr);
            }
            replacement = expr;
        } else {
            replacement = xExpr;
            if (warnings) {
                warnings->append(QString("LT table(...) include/file form is not supported; approximated by passing through the lookup input expression in: %1").arg(line.trimmed()));
            }
        }

        out.replace(tableIndex, closeIndex - tableIndex + 1, replacement);
        changed = true;
    }

    if (changed && warnings) {
        warnings->append(QString("Approximated LT table(...) with nested conditional interpolation for ngspice compatibility in: %1").arg(line.trimmed()));
    }
    return out;
}

QString SpiceCompatRewriter::buildCurrentTableExpr(const QString& xExpr, const QStringList& args, QString* error) {
    if (args.size() < 2 || (args.size() % 2) != 0) {
        if (error) *error = "Current-source table requires voltage/current pairs.";
        return QString();
    }

    QString expr = args.last().trimmed();
    for (int i = args.size() - 4; i >= 0; i -= 2) {
        const QString x0 = args.at(i).trimmed();
        const QString y0 = args.at(i + 1).trimmed();
        const QString x1 = args.at(i + 2).trimmed();
        const QString y1 = args.at(i + 3).trimmed();
        QString segment = y1;
        if (x0.compare(x1, Qt::CaseInsensitive) != 0) {
            segment = QString("((%1)+((%2)-(%3))*(((%4)-(%5))/((%6)-(%7))))")
                          .arg(y0, y1, y0, xExpr, x0, x1, x0);
        }
        expr = QString("if((%1)<=(%2),(%3),(%4))").arg(xExpr, x1, segment, expr);
    }
    return expr;
}

QString SpiceCompatRewriter::rewriteUnsupportedLtStochasticFunctions(const QString& line, QStringList* warnings) {
    QString out = line;
    struct FuncSpec { QString name; int minArgs; int maxArgs; QString replacement; };
    const QList<FuncSpec> funcs = {
        {"rand", 1, 1, "0"},
        {"random", 1, 1, "0"},
        {"white", 1, 1, "0"},
        {"smallsig", 0, 0, "0"},
    };

    bool replaced = true;
    while (replaced) {
        replaced = false;
        for (const FuncSpec& func : funcs) {
            const int funcIndex = out.indexOf(QRegularExpression(QString("\\b%1\\s*\\(").arg(QRegularExpression::escape(func.name)),
                                                                   QRegularExpression::CaseInsensitiveOption));
            if (funcIndex < 0) continue;
            const int openIndex = out.indexOf('(', funcIndex);
            const int closeIndex = findMatchingParen(out, openIndex);
            if (openIndex < 0 || closeIndex < 0) continue;
            const QString inner = out.mid(openIndex + 1, closeIndex - openIndex - 1);
            const QStringList args = inner.trimmed().isEmpty() ? QStringList() : splitTopLevelSpiceArgs(inner);
            if (args.size() < func.minArgs || args.size() > func.maxArgs) continue;

            out.replace(funcIndex, closeIndex - funcIndex + 1, func.replacement);
            replaced = true;
            if (warnings) {
                warnings->append(QString("Approximated LT %1(...) as 0 because this ngspice configuration does not support %1(...). Original line: %2")
                                     .arg(func.name, line.trimmed()));
            }
            break;
        }
    }

    return out;
}

QString SpiceCompatRewriter::rewriteLtBSourceLaplaceOptions(const QString& line, QStringList* warnings) {
    static const QRegularExpression bSourceRe(
        "^\\s*(B\\S+)\\s+\\S+\\s+\\S+\\s+(?:V|I|R|P)\\s*=.*$",
        QRegularExpression::CaseInsensitiveOption);
    if (!bSourceRe.match(line).hasMatch()) return line;

    QString out = line;
    const QString original = line.trimmed();

    static const QRegularExpression laplaceRe("\\blaplace\\s*=\\s*(\\{[^}]*\\}|\\S+)", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression windowRe("\\bwindow\\s*=\\s*\\S+", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression nfftRe("\\bnfft\\s*=\\s*\\S+", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression mtolRe("\\bmtol\\s*=\\s*\\S+", QRegularExpression::CaseInsensitiveOption);

    const QRegularExpressionMatch laplaceMatch = laplaceRe.match(out);
    if (!laplaceMatch.hasMatch()) return line;

    const QString laplaceExpr = laplaceMatch.captured(1).trimmed();
    out.remove(laplaceRe);
    out.remove(windowRe);
    out.remove(nfftRe);
    out.remove(mtolRe);
    out = out.simplified();

    if (warnings) {
        warnings->append(QString("Dropped LT B-source laplace= transform from %1 because this ngspice configuration does not accept LT-style Laplace options on B-sources.").arg(original));
        warnings->append(QString("Preserved the underlying behavioral source but removed laplace/window/nfft/mtol options; resulting behavior may differ from LT. Dropped Laplace expression: %1").arg(laplaceExpr));
    }

    return out;
}

QString SpiceCompatRewriter::rewriteLtBehavioralSourceRpar(const QString& line, QStringList* warnings) {
    static const QRegularExpression bSourceRe(
        "^\\s*(B\\S+)\\s+(\\S+)\\s+(\\S+)\\s+([IR])\\s*=\\s*(.+)$",
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = bSourceRe.match(line);
    if (!match.hasMatch()) return line;

    QString out = line;
    const QString ref = match.captured(1).trimmed();
    const QString nplus = match.captured(2).trimmed();
    const QString nminus = match.captured(3).trimmed();
    const QString mode = match.captured(4).trimmed().toUpper();
    QString exprAndTail = match.captured(5).trimmed();

    static const QRegularExpression rparRe("\\bRpar\\s*=\\s*([^\\s]+)", QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch rparMatch = rparRe.match(exprAndTail);
    if (!rparMatch.hasMatch()) return line;

    const QString rparValue = rparMatch.captured(1).trimmed();
    exprAndTail.remove(rparRe);
    exprAndTail = exprAndTail.simplified();

    const QString shuntRef = QString("R__RPAR_%1").arg(ref);
    QStringList rewrittenLines;
    rewrittenLines << QString("%1 %2 %3 %4=%5").arg(ref, nplus, nminus, mode, exprAndTail);
    rewrittenLines << QString("%1 %2 %3 %4").arg(shuntRef, nplus, nminus, rparValue);
    out = rewrittenLines.join("\n");

    if (warnings) {
        warnings->append(QString("Expanded LT behavioral source Rpar= on %1 into explicit shunt resistor for ngspice.").arg(ref));
    }
    return out;
}

QString SpiceCompatRewriter::rewriteLtSourceTripOptions(const QString& line, QStringList* warnings) {
    static const QRegularExpression sourceRe(
        "^\\s*([VI]\\S*)\\s+(\\S+)\\s+(\\S+)\\s+(.+)$",
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = sourceRe.match(line);
    if (!match.hasMatch()) return line;

    const QString ref = match.captured(1).trimmed();
    QString rest = match.captured(4).trimmed();
    if (!(rest.startsWith("PULSE", Qt::CaseInsensitive) || rest.startsWith("PWL", Qt::CaseInsensitive) ||
          rest.startsWith("SINE", Qt::CaseInsensitive) || rest.startsWith("EXP", Qt::CaseInsensitive) ||
          rest.startsWith("SFFM", Qt::CaseInsensitive))) {
        return line;
    }

    static const QRegularExpression tripdvRe("\\btripdv\\s*=\\s*([^\\s]+)", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression tripdtRe("\\btripdt\\s*=\\s*([^\\s]+)", QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch tripdvMatch = tripdvRe.match(rest);
    const QRegularExpressionMatch tripdtMatch = tripdtRe.match(rest);
    if (!tripdvMatch.hasMatch() && !tripdtMatch.hasMatch()) return line;

    const QString tripdv = tripdvMatch.hasMatch() ? tripdvMatch.captured(1).trimmed() : QString();
    const QString tripdt = tripdtMatch.hasMatch() ? tripdtMatch.captured(1).trimmed() : QString();
    rest.remove(tripdvRe);
    rest.remove(tripdtRe);
    rest = rest.simplified();

    const QString out = QString("%1 %2 %3 %4")
                            .arg(ref, match.captured(2).trimmed(), match.captured(3).trimmed(), rest);
    if (warnings) {
        warnings->append(QString("Dropped LT source tripdv=/tripdt= options from %1 because this ngspice configuration rejects them on independent sources.").arg(ref));
        warnings->append(QString("Removed step-rejection options from %1: tripdv=%2 tripdt=%3").arg(
            ref,
            tripdv.isEmpty() ? QString("<none>") : tripdv,
            tripdt.isEmpty() ? QString("<none>") : tripdt));
    }
    return out;
}

QString SpiceCompatRewriter::rewriteLtBSourceTripOptions(const QString& line, QStringList* warnings) {
    static const QRegularExpression bSourceRe(
        "^\\s*(B\\S+)\\s+(\\S+)\\s+(\\S+)\\s+([VIRP])\\s*=\\s*(.+)$",
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = bSourceRe.match(line);
    if (!match.hasMatch()) return line;

    const QString ref = match.captured(1).trimmed();
    QString tail = match.captured(5).trimmed();

    static const QRegularExpression tripdvRe("\\btripdv\\s*=\\s*([^\\s]+)", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression tripdtRe("\\btripdt\\s*=\\s*([^\\s]+)", QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch tripdvMatch = tripdvRe.match(tail);
    const QRegularExpressionMatch tripdtMatch = tripdtRe.match(tail);
    if (!tripdvMatch.hasMatch() && !tripdtMatch.hasMatch()) return line;

    const QString tripdv = tripdvMatch.hasMatch() ? tripdvMatch.captured(1).trimmed() : QString();
    const QString tripdt = tripdtMatch.hasMatch() ? tripdtMatch.captured(1).trimmed() : QString();
    tail.remove(tripdvRe);
    tail.remove(tripdtRe);
    tail = tail.simplified();

    const QString out = QString("%1 %2 %3 %4=%5")
                            .arg(ref, match.captured(2).trimmed(), match.captured(3).trimmed(), match.captured(4).trimmed(), tail);
    if (warnings) {
        warnings->append(QString("Dropped LT B-source tripdv=/tripdt= options from %1 because this ngspice configuration rejects them on behavioral sources.").arg(ref));
        warnings->append(QString("Removed B-source step-rejection options from %1: tripdv=%2 tripdt=%3").arg(
            ref,
            tripdv.isEmpty() ? QString("<none>") : tripdv,
            tripdt.isEmpty() ? QString("<none>") : tripdt));
    }
    return out;
}

void SpiceCompatRewriter::appendLtBSourceOptionWarnings(const QString& line, QStringList* warnings) {
    if (!warnings) return;

    static const QRegularExpression bSourceRe(
        "^\\s*(B\\S+)\\s+\\S+\\s+\\S+\\s+(?:V|I|R|P)\\s*=.*$",
        QRegularExpression::CaseInsensitiveOption);
    if (!bSourceRe.match(line).hasMatch()) return;

    const QString trimmed = line.trimmed();
    if (trimmed.contains(QRegularExpression("\\bic\\s*=", QRegularExpression::CaseInsensitiveOption))) {
        warnings->append(QString("LT B-source instance option ic= detected and passed through unchanged: %1").arg(trimmed));
    }
    if (trimmed.contains(QRegularExpression("\\bvprx\\s*=", QRegularExpression::CaseInsensitiveOption))) {
        warnings->append(QString("LT behavioral power-source option vprx= detected and passed through unchanged; ngspice compatibility may differ: %1").arg(trimmed));
    }
    if (trimmed.contains(QRegularExpression("\\btripdv\\s*=", QRegularExpression::CaseInsensitiveOption)) ||
        trimmed.contains(QRegularExpression("\\btripdt\\s*=", QRegularExpression::CaseInsensitiveOption))) {
        warnings->append(QString("LT B-source step-rejection options tripdv=/tripdt= detected; VioSpice will drop them if needed to keep ngspice loadable: %1").arg(trimmed));
    }
    if (trimmed.contains(QRegularExpression("\\blaplace\\s*=", QRegularExpression::CaseInsensitiveOption)) ||
        trimmed.contains(QRegularExpression("\\bwindow\\s*=", QRegularExpression::CaseInsensitiveOption)) ||
        trimmed.contains(QRegularExpression("\\bnfft\\s*=", QRegularExpression::CaseInsensitiveOption)) ||
        trimmed.contains(QRegularExpression("\\bmtol\\s*=", QRegularExpression::CaseInsensitiveOption))) {
        warnings->append(QString("LT B-source Laplace options detected; VioSpice will drop them if needed to keep ngspice loadable: %1").arg(trimmed));
    }
}

void SpiceCompatRewriter::appendLtSourceOptionWarnings(const QString& line, QStringList* warnings) {
    if (!warnings) return;

    static const QRegularExpression sourceRe(
        "^\\s*([VI]\\S*)\\s+\\S+\\s+\\S+\\s+(.+)$",
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = sourceRe.match(line);
    if (!match.hasMatch()) return;

    const QString ref = match.captured(1).trimmed();
    const QString rest = match.captured(2).trimmed();
    if (!(rest.startsWith("PULSE", Qt::CaseInsensitive) || rest.startsWith("PWL", Qt::CaseInsensitive) ||
          rest.startsWith("SINE", Qt::CaseInsensitive) || rest.startsWith("EXP", Qt::CaseInsensitive) ||
          rest.startsWith("SFFM", Qt::CaseInsensitive))) {
        return;
    }

    if (rest.contains(QRegularExpression("\\bTrigger\\s*=", QRegularExpression::CaseInsensitiveOption))) {
        if (rest.startsWith("PULSE", Qt::CaseInsensitive)) {
            warnings->append(QString("LT PULSE Trigger= detected on %1; VioSpice will approximate it by gating a hidden pulse source.").arg(ref));
        } else if (rest.startsWith("PWL", Qt::CaseInsensitive)) {
            warnings->append(QString("LT PWL Trigger= detected on %1; VioSpice will approximate it by gating a hidden PWL source.").arg(ref));
        } else if (rest.startsWith("SINE", Qt::CaseInsensitive) || rest.startsWith("EXP", Qt::CaseInsensitive) ||
                   rest.startsWith("SFFM", Qt::CaseInsensitive)) {
            warnings->append(QString("LT %1 Trigger= detected on %2; VioSpice will approximate it by gating a hidden %1 source.")
                                 .arg(rest.section('(', 0, 0).trimmed().toUpper(), ref));
        } else {
            warnings->append(QString("LT triggered source restart semantics are not yet emulated for %1; Trigger= is passed through unchanged: %2").arg(ref, line.trimmed()));
        }
    }
    if (rest.contains(QRegularExpression("\\btripdv\\s*=", QRegularExpression::CaseInsensitiveOption)) ||
        rest.contains(QRegularExpression("\\btripdt\\s*=", QRegularExpression::CaseInsensitiveOption))) {
        warnings->append(QString("LT source step-rejection options tripdv=/tripdt= detected on %1; VioSpice will drop them if needed to keep ngspice loadable: %2").arg(ref, line.trimmed()));
    }
}

QString SpiceCompatRewriter::rewriteLtStartupSourceLine(const QString& line, QStringList* warnings) {
    static const QString startupScaleExpr = "min(1,max(0,time/20u))";
    static const QRegularExpression simpleValueRe(
        "^(?:DC\\s+)?(\\{[^}]+\\}|[-+]?\\d+(?:\\.\\d+)?(?:[eE][-+]?\\d+)?[a-zA-Z]*)$",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression voltageSourceRe(
        "^\\s*(V\\S*)\\s+(\\S+)\\s+(\\S+)\\s+(.+?)\\s*$",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression currentDcSourceRe(
        "^\\s*(I\\S*)\\s+(\\S+)\\s+(\\S+)\\s+(?:DC\\s+)?(\\{[^}]+\\}|[-+]?\\d+(?:\\.\\d+)?(?:[eE][-+]?\\d+)?[a-zA-Z]*)\\s*$",
        QRegularExpression::CaseInsensitiveOption);

    const QRegularExpressionMatch voltageMatch = voltageSourceRe.match(line);
    if (voltageMatch.hasMatch()) {
        const QString ref = voltageMatch.captured(1).trimmed();
        const QString nplus = voltageMatch.captured(2).trimmed();
        const QString nminus = voltageMatch.captured(3).trimmed();
        const QString value = voltageMatch.captured(4).trimmed();

        const QRegularExpressionMatch simpleValueMatch = simpleValueRe.match(value);
        if (simpleValueMatch.hasMatch()) {
            const QString targetValue = simpleValueMatch.captured(1).trimmed();
            const QString rewritten = QString("%1 %2 %3 PWL(0 0 20u %4)").arg(ref, nplus, nminus, targetValue);
            if (warnings) {
                warnings->append(QString("Approximated LT startup behavior by ramping voltage source %1 from 0 to its target over 20us.").arg(ref));
            }
            return rewritten;
        }

        const QString hiddenNode = QString("%1__startup").arg(ref);
        const QString hiddenRef = QString("V__STARTUPSRC_%1").arg(ref);
        const QString bufferRef = QString("B__STARTUPBUF_%1").arg(ref);

        QStringList rewrittenLines;
        rewrittenLines << QString("%1 %2 %3 %4").arg(hiddenRef, hiddenNode, nminus, value);
        rewrittenLines << QString("%1 %2 %3 V={(%4)*V(%5,%6)}").arg(bufferRef, nplus, nminus, startupScaleExpr, hiddenNode, nminus);
        if (warnings) {
            warnings->append(QString("Approximated LT startup behavior by scaling voltage source %1 from 0 to full amplitude over 20us.").arg(ref));
        }
        return rewrittenLines.join("\n");
    }

    const QRegularExpressionMatch currentMatch = currentDcSourceRe.match(line);
    if (!currentMatch.hasMatch()) return line;

    const QString ref = currentMatch.captured(1).trimmed();
    const QString nplus = currentMatch.captured(2).trimmed();
    const QString nminus = currentMatch.captured(3).trimmed();
    const QString value = currentMatch.captured(4).trimmed();
    const QString rewritten = QString("%1 %2 %3 PWL(0 0 20u %4)").arg(ref, nplus, nminus, value);
    if (warnings) {
        warnings->append(QString("Approximated LT startup behavior by ramping current source %1 from 0 to its target over 20us.").arg(ref));
    }
    return rewritten;
}

QString SpiceCompatRewriter::rewriteLtDirectiveLine(const QString& line, QStringList* warnings, bool emulateStartup, const QString& projectDir) {
    QString out = line;

    appendLtBSourceOptionWarnings(out, warnings);
    appendLtSourceOptionWarnings(out, warnings);

    out = rewriteLtBSourceTripOptions(out, warnings);
    out = rewriteLtBSourceLaplaceOptions(out, warnings);
    out = rewriteLtBehavioralSourceRpar(out, warnings);
    out = rewriteLtSourceTripOptions(out, warnings);
    out = rewriteLtTriggeredPulseSource(out, warnings);
    out = rewriteLtTriggeredPwlSource(out, warnings);
    out = rewriteLtTriggeredWaveSource(out, "SINE", warnings);
    out = rewriteLtTriggeredWaveSource(out, "EXP", warnings);
    out = rewriteLtTriggeredWaveSource(out, "SFFM", warnings);
    out = rewriteLtVoltageSourceExtras(out, warnings);
    out = rewriteWavefileSourceSyntax(out, warnings);

    {
        static const QRegularExpression sourceRe(
            "^\\s*([VI]\\S*)\\s+(\\S+)\\s+(\\S+)\\s+(.+)$",
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch sourceMatch = sourceRe.match(out);
        if (sourceMatch.hasMatch()) {
            const QString ref = sourceMatch.captured(1);
            const QString nplus = sourceMatch.captured(2);
            const QString nminus = sourceMatch.captured(3);
            const QString prefix = QString("%1 %2 %3 ").arg(ref, nplus, nminus);
            const QString rest = sourceMatch.captured(4).trimmed();
            if (ref.startsWith('I', Qt::CaseInsensitive)) {
                QString rewritten;
                if (rewriteLtCurrentSourceSpecial(ref, nplus, nminus, rest, projectDir, &rewritten, warnings)) {
                    out = rewritten;
                    return out;
                }
            }

            // Optimization: If 'rest' already looks like expanded PWL (starts with PWL and contains
            // many tokens or newlines), don't try to inline it again.
            if (rest.startsWith("PWL", Qt::CaseInsensitive) && (rest.contains('\n') || rest.split(' ').size() > 10)) {
                return out;
            }

            const QString normalizedRest = inlinePwlFileIfNeeded(rest, projectDir, warnings);
            out = prefix + normalizedRest;
        }
    }

    if (emulateStartup) {
        QStringList startupLines;
        for (const QString& part : out.split('\n')) {
            startupLines << rewriteLtStartupSourceLine(part, warnings);
        }
        out = startupLines.join("\n");
    }

    if (out.trimmed().compare(".end", Qt::CaseInsensitive) == 0) {
        if (warnings) {
            warnings->append(QString("Dropped .end from directive block; VioSpice appends the final .end automatically."));
        }
        return QString();
    }

    {
        static const QRegularExpression bSourceRe(
            "^\\s*(B\\S+)\\s+(\\S+)\\s+(\\S+)\\s+V\\s*=\\s*(.+)$",
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch bMatch = bSourceRe.match(out);
        if (bMatch.hasMatch() && (out.contains("idt(", Qt::CaseInsensitive) || out.contains("sdt(", Qt::CaseInsensitive) || out.contains("idtmod(", Qt::CaseInsensitive))) {
            const QString ref = bMatch.captured(1).trimmed();
            const QString nplus = bMatch.captured(2).trimmed();
            const QString nminus = bMatch.captured(3).trimmed();
            QString expr = bMatch.captured(4).trimmed();

            // Strip outer braces if present for cleaner parsing
            bool hadBraces = false;
            if (expr.startsWith('{') && expr.endsWith('}')) {
                expr = expr.mid(1, expr.length() - 2).trimmed();
                hadBraces = true;
            }

            static const QRegularExpression idtTailRe(
                "^(.*?)(?:([+\\-])\\s*)?([^\\s+\\-]+)?\\s*\\*?\\s*(idt|sdt|idtmod)\\s*\\((.*)\\)\\s*$",
                QRegularExpression::CaseInsensitiveOption);
            const QRegularExpressionMatch idtMatch = idtTailRe.match(expr);
            if (idtMatch.hasMatch()) {
                QString prefix = idtMatch.captured(1).trimmed();
                const QString sign = idtMatch.captured(2).trimmed();
                QString coeff = idtMatch.captured(3).trimmed();
                const QString funcName = idtMatch.captured(4).trimmed();
                const QStringList idtArgs = splitTopLevelSpiceArgs(idtMatch.captured(5).trimmed());
                const QString innerExpr = idtArgs.value(0).trimmed();
                const QString icExpr = idtArgs.value(1).trimmed().isEmpty() ? "0" : idtArgs.value(1).trimmed();
                const bool isIdtMod = funcName.compare("idtmod", Qt::CaseInsensitive) == 0;
                const QString assertExpr = isIdtMod ? QString() : idtArgs.value(2).trimmed();
                const QString modulusExpr = isIdtMod ? (idtArgs.value(2).trimmed().isEmpty() ? "1" : idtArgs.value(2).trimmed()) : QString();
                const QString offsetExpr = isIdtMod ? (idtArgs.value(3).trimmed().isEmpty() ? "0" : idtArgs.value(3).trimmed()) : QString();
                if (innerExpr.isEmpty()) return out;
                coeff.remove(QRegularExpression("\\*+$"));
                if (coeff.isEmpty()) coeff = "1";
                if (sign == "-") coeff = QString("-(%1)").arg(coeff);
                const QString intNode = QString("%1__idt").arg(ref);
                const QString intDrvRef = QString("B__INTDRV_%1").arg(ref);
                const QString intCapRef = QString("C__INT_%1").arg(ref);
                const QString intLeakRef = QString("R__INTLEAK_%1").arg(ref);
                const QString intResetRef = QString("B__INTRESET_%1").arg(ref);

                QString driveExpr = QString("(%1)*(%2)").arg(coeff, innerExpr);
                if (!assertExpr.isEmpty()) {
                    const QString resetGateExpr = QString("u((%1)-(0.5))").arg(assertExpr);
                    driveExpr = QString("(1-(%1))*(%2)").arg(resetGateExpr, driveExpr);
                }

                QString rewrittenExpr = prefix;
                if (rewrittenExpr.isEmpty()) {
                    if (isIdtMod) {
                        rewrittenExpr = QString("((%1)+((V(%2)-(%1))-(%3)*floor(((V(%2)-(%1))/(%3)))))")
                                            .arg(offsetExpr, intNode, modulusExpr);
                    } else {
                        rewrittenExpr = QString("V(%1)").arg(intNode);
                    }
                } else {
                    // Ensure prefix doesn't have unbalanced braces after split
                    if (prefix.count('{') > prefix.count('}')) prefix += "}";
                    if (prefix.count('}') > prefix.count('{')) prefix.prepend("{");
                    if (isIdtMod) {
                        rewrittenExpr = prefix + QString(" + ((%1)+((V(%2)-(%1))-(%3)*floor(((V(%2)-(%1))/(%3)))))")
                                                   .arg(offsetExpr, intNode, modulusExpr);
                    } else {
                        rewrittenExpr = prefix + QString(" + V(%1)").arg(intNode);
                    }
                }

                // If the original had braces, ensure the new one does too (handled by the bExprRe block usually,
                // but we might be multiline now, so bExprRe won't match the whole 'out')
                if (hadBraces && !rewrittenExpr.startsWith('{')) {
                    rewrittenExpr = "{" + rewrittenExpr + "}";
                }

                QStringList rewrittenLines;
                rewrittenLines << QString("%1 0 %2 I=%3").arg(intDrvRef, intNode, driveExpr);
                if (!assertExpr.isEmpty()) {
                    const QString resetGateExpr = QString("u((%1)-(0.5))").arg(assertExpr);
                    rewrittenLines << QString("%1 0 %2 I=(%3)*(1e6)*((%4)-V(%2))").arg(intResetRef, intNode, resetGateExpr, icExpr);
                }
                rewrittenLines << QString("%1 %2 0 1").arg(intCapRef, intNode);
                rewrittenLines << QString("%1 %2 0 1G").arg(intLeakRef, intNode);
                rewrittenLines << QString(".ic V(%1)=%2").arg(intNode, icExpr);
                rewrittenLines << QString("%1 %2 %3 V=%4").arg(ref, nplus, nminus, rewrittenExpr);
                out = rewrittenLines.join("\n");
                if (warnings) {
                    warnings->append(QString("Expanded LT %1(...) in %2 into an explicit behavioral integrator for ngspice.").arg(funcName.toLower(), ref));
                    if (idtArgs.size() >= 2) {
                        warnings->append(QString("Preserved LT %1 initial condition for %2 as %3.").arg(funcName.toLower(), ref, icExpr));
                    }
                    if (!assertExpr.isEmpty()) {
                        warnings->append(QString("Approximated LT %1 reset/assert argument for %2 using a behavioral reset clamp.").arg(funcName.toLower(), ref));
                    }
                    if (isIdtMod) {
                        warnings->append(QString("Approximated LT idtmod(...) for %1 by wrapping the explicit integrator output with modulus %2 and offset %3.").arg(ref, modulusExpr, offsetExpr));
                    }
                }
            }
        }
    }

    {
        // Handle multiline B-sources if idt expansion happened
        QStringList lines = out.split('\n');
        for (int i = 0; i < lines.size(); ++i) {
            static const QRegularExpression bExprRe(
                "^\\s*(B\\S+\\s+\\S+\\s+\\S+\\s+)([VI])\\s*=\\s*(.+)$",
                QRegularExpression::CaseInsensitiveOption);
            const QRegularExpressionMatch bExprMatch = bExprRe.match(lines[i]);
            if (bExprMatch.hasMatch()) {
                const QString head = bExprMatch.captured(1);
                const QString kind = bExprMatch.captured(2);
                QString expr = bExprMatch.captured(3).trimmed();

                // Normalize logical operators: ngspice prefers && and || for logic
                expr.replace(" & ", " && ");
                expr.replace(" | ", " || ");

                if (!(expr.startsWith('{') && expr.endsWith('}'))) {
                    lines[i] = QString("%1%2={%3}").arg(head, kind, expr);
                    if (warnings && !out.contains("\n")) { // Only warn once for simple lines
                        warnings->append(QString("Wrapped LT-style behavioral source expression in braces for ngspice: %1").arg(line.trimmed()));
                    }
                }
            }
        }
        out = lines.join("\n");
    }

    if (out.contains("if(", Qt::CaseInsensitive) && out.contains("={", Qt::CaseInsensitive)) {
        out = rewriteLtBehavioralIf(out, warnings);
    }

    if ((out.contains("buf(", Qt::CaseInsensitive) || out.contains("inv(", Qt::CaseInsensitive) ||
         out.contains("uramp(", Qt::CaseInsensitive) || out.contains("limit(", Qt::CaseInsensitive) ||
         out.contains("dnlim(", Qt::CaseInsensitive) || out.contains("uplim(", Qt::CaseInsensitive) ||
         out.contains("idtmod(", Qt::CaseInsensitive)) && out.contains("={", Qt::CaseInsensitive)) {
        out = rewriteLtBehavioralFunctions(out, warnings);
    }

    if ((out.contains("delay(", Qt::CaseInsensitive) || out.contains("absdelay(", Qt::CaseInsensitive)) && out.contains("={", Qt::CaseInsensitive)) {
        out = rewriteUnsupportedLtBehavioralTimeFunctions(out, warnings);
    }

    if (out.contains("table(", Qt::CaseInsensitive) && out.contains("={", Qt::CaseInsensitive)) {
        out = rewriteUnsupportedLtTableFunction(out, warnings);
        if (out.contains("if(", Qt::CaseInsensitive)) {
            out = rewriteLtBehavioralIf(out, warnings);
        }
    }

    if ((out.contains("rand(", Qt::CaseInsensitive) || out.contains("random(", Qt::CaseInsensitive) ||
         out.contains("white(", Qt::CaseInsensitive) || out.contains("smallsig(", Qt::CaseInsensitive)) &&
        out.contains("={", Qt::CaseInsensitive)) {
        out = rewriteUnsupportedLtStochasticFunctions(out, warnings);
    }

    if (out.contains(" V={", Qt::CaseInsensitive)) {
        const QString original = out;
        out.replace("&&", " and ");
        out.replace("||", " or ");

        static const QRegularExpression singleAndRe("(?<![&])&(?![&])");
        static const QRegularExpression singleOrRe("(?<![|])\\|(?![|])");
        out.replace(singleAndRe, " and ");
        out.replace(singleOrRe, " or ");

        if (out != original && warnings) {
            warnings->append(QString("Rewrote LT-style boolean operators to ngspice-safe logical operators in: %1").arg(line.trimmed()));
        }
    }

    {
        static const QRegularExpression passiveRserRe(
            "^\\s*([CL][^\\s]*)\\s+(\\S+)\\s+(\\S+)\\s+(\\S+)(.*\\bRser\\s*=\\s*([^\\s]+).*)$",
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch passiveMatch = passiveRserRe.match(out);
        if (passiveMatch.hasMatch()) {
            const QString ref = passiveMatch.captured(1).trimmed();
            const QString node1 = passiveMatch.captured(2).trimmed();
            const QString node2 = passiveMatch.captured(3).trimmed();
            const QString value = passiveMatch.captured(4).trimmed();
            QString extras = passiveMatch.captured(5);
            const QString rser = passiveMatch.captured(6).trimmed();

            extras.remove(QRegularExpression("\\bRser\\s*=\\s*([^\\s]+)", QRegularExpression::CaseInsensitiveOption));
            extras = extras.simplified();

            const QString insertedNode = QString("%1__rser").arg(ref);
            const QString seriesRef = QString("R__RSER_%1").arg(ref);
            QStringList rewrittenLines;
            rewrittenLines << QString("%1 %2 %3 %4").arg(seriesRef, node1, insertedNode, rser);

            QString passiveLine = QString("%1 %2 %3 %4").arg(ref, insertedNode, node2, value);
            if (!extras.isEmpty()) passiveLine += " " + extras;
            rewrittenLines << passiveLine;
            out = rewrittenLines.join("\n");
            if (warnings) {
                warnings->append(QString("Expanded LT inline Rser= on %1 into explicit series resistor for ngspice.").arg(ref));
            }
        }
    }

    {
        static const QRegularExpression diodeModelRe(
            "^\\s*\\.model\\s+(\\S+)\\s+D\\((.*)\\)\\s*$",
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch diodeMatch = diodeModelRe.match(out);
        if (diodeMatch.hasMatch()) {
            QString body = diodeMatch.captured(2);
            const QString originalBody = body;
            body.replace(QRegularExpression("\\bRon\\s*=", QRegularExpression::CaseInsensitiveOption), "Rs=");
            body.remove(QRegularExpression("(?:^|\\s)Roff\\s*=\\s*[^\\s)]+", QRegularExpression::CaseInsensitiveOption));
            body.remove(QRegularExpression("(?:^|\\s)Vfwd\\s*=\\s*[^\\s)]+", QRegularExpression::CaseInsensitiveOption));
            body = body.simplified();
            if (!body.contains(QRegularExpression("\\bIs\\s*=", QRegularExpression::CaseInsensitiveOption))) {
                body.prepend("Is=1e-14 ");
            }
            if (!body.contains(QRegularExpression("\\bN\\s*=", QRegularExpression::CaseInsensitiveOption))) {
                body += " N=1";
            }
            body = body.simplified();
            out = QString(".model %1 D(%2)").arg(diodeMatch.captured(1), body);
            if (warnings && body != originalBody.simplified()) {
                warnings->append(QString("Rewrote LT-style diode model parameters for ngspice in: %1").arg(line.trimmed()));
            }
        }
    }

    {
        static const QRegularExpression tranRe(
            "^\\s*\\.tran\\s+(\\S+)\\s+(\\S+)(?:\\s+(\\S+))?(?:\\s+(\\S+))?(.*)$",
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch tranMatch = tranRe.match(out);
        if (tranMatch.hasMatch()) {
            const QString tstep = tranMatch.captured(1).trimmed();
            const QString tstop = tranMatch.captured(2).trimmed();
            QString tstart = tranMatch.captured(3).trimmed();
            QString tmax = tranMatch.captured(4).trimmed();
            QString tail = tranMatch.captured(5).simplified();

            auto promoteModifierToken = [&tail](QString& token) {
                if (token.compare("startup", Qt::CaseInsensitive) == 0 ||
                    token.compare("uic", Qt::CaseInsensitive) == 0) {
                    if (!tail.isEmpty()) tail.prepend(token + " ");
                    else tail = token;
                    token.clear();
                }
            };
            promoteModifierToken(tstart);
            promoteModifierToken(tmax);

            // Strip LT-only 'startup'; source ramping is handled separately.
            bool changed = false;
            if (tail.contains("startup", Qt::CaseInsensitive)) {
                tail.remove("startup", Qt::CaseInsensitive);
                tail = tail.trimmed();
                changed = true;
                if (warnings) {
                    warnings->append(QString("Removed LT 'startup' keyword from .tran and approximated it by ramping top-level independent sources over the first 20us."));
                }
            }

            if ((tstep == "0" || tstep == "0.0") && !tmax.isEmpty()) {
                out = QString(".tran %1 %2").arg(tmax, tstop);
                if (!tstart.isEmpty()) out += " " + tstart;
                out += " " + tmax; // Re-add tmax as the 4th parameter for ngspice tmax behavior
                if (!tail.isEmpty()) out += " " + tail;
                if (warnings) {
                    warnings->append(QString("Rewrote .tran with zero print step to preserve LT tmax behavior for ngspice: %1").arg(line.trimmed()));
                }
            } else if (changed || !tail.isEmpty() || out != tranMatch.captured(0)) {
                // Update the line to reflect stripped startup or other changes
                out = QString(".tran %1 %2").arg(tstep, tstop);
                if (!tstart.isEmpty()) out += " " + tstart;
                if (!tmax.isEmpty()) out += " " + tmax;
                if (!tail.isEmpty()) out += " " + tail;
            }
        }
    }

    if (out.startsWith(".include ", Qt::CaseInsensitive) || out.startsWith(".lib ", Qt::CaseInsensitive) || out.startsWith(".inc ", Qt::CaseInsensitive)) {
        static const QRegularExpression incRe("^(?:\\.(?:include|lib|inc))\\s+(?:\"?)(spice/[^\"]+)(?:\"?)\\s*$", QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch incMatch = incRe.match(out);
        if (incMatch.hasMatch()) {
            QString keyword = out.left(out.indexOf(' '));
            QString oldPath = incMatch.captured(1);
            QString newPath = "sub/" + oldPath.mid(6);
            out = QString("%1 \"%2\"").arg(keyword, newPath);
            if (warnings) {
                warnings->append(QString("Auto-corrected legacy include path from %1 to %2 for backwards compatibility.").arg(oldPath, newPath));
            }
        }
    }

    return out;
}

QStringList SpiceCompatRewriter::collapseSpiceContinuationLines(const QString& text) {
    QStringList collapsed;
    QString current;

    const QStringList lines = text.split('\n');
    for (const QString& rawLine : lines) {
        const QString trimmed = rawLine.trimmed();

        if (trimmed.startsWith('+')) {
            const QString continuation = trimmed.mid(1).trimmed();
            if (current.isEmpty()) {
                current = continuation;
            } else if (!continuation.isEmpty()) {
                if (!current.endsWith(' ')) current += ' ';
                current += continuation;
            }
            continue;
        }

        if (!current.isEmpty()) {
            collapsed.append(current);
        }
        current = rawLine;
    }
    if (!current.isEmpty()) {
        collapsed.append(current);
    }
    return collapsed;
}

QStringList SpiceCompatRewriter::splitTopLevelSpiceArgs(const QString& text) {
    QStringList args;
    QString current;
    int parenDepth = 0;
    int braceDepth = 0;

    for (QChar ch : text) {
        if (ch == ',' && parenDepth == 0 && braceDepth == 0) {
            args.append(current.trimmed());
            current.clear();
            continue;
        }
        if (ch == '(') ++parenDepth;
        else if (ch == ')' && parenDepth > 0) --parenDepth;
        else if (ch == '{') ++braceDepth;
        else if (ch == '}' && braceDepth > 0) --braceDepth;
        current += ch;
    }
    args.append(current.trimmed());
    return args;
}

int SpiceCompatRewriter::findMatchingParen(const QString& text, int openIndex) {
    if (openIndex < 0 || openIndex >= text.size() || text.at(openIndex) != '(') return -1;
    int depth = 0;
    for (int i = openIndex; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        if (ch == '(') ++depth;
        else if (ch == ')') {
            --depth;
            if (depth == 0) return i;
        }
    }
    return -1;
}
