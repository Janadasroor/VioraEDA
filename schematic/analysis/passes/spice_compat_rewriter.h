/*
 * Copyright 2026 Janada Sroor
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SPICE_COMPAT_REWRITER_H
#define SPICE_COMPAT_REWRITER_H

#include <QString>
#include <QStringList>
#include <QMap>
#include <QSet>

class SpiceCompatRewriter {
public:
    static bool convertLtConditionToStepExpr(const QString& condition, QString* stepExpr);
    static void updateSubcktDepthForLine(const QString& line, int& subcktDepth);
    static QString rewriteLtBehavioralIf(const QString& line, QStringList* warnings = nullptr);
    static QString rewriteLtVoltageSourceExtras(const QString& line, QStringList* warnings = nullptr);
    static QString rewriteWavefileSourceSyntax(const QString& line, QStringList* warnings = nullptr);
    static QString rewriteLtTriggeredPulseSource(const QString& line, QStringList* warnings = nullptr);
    static QString rewriteLtTriggeredPwlSource(const QString& line, QStringList* warnings = nullptr);
    static QString rewriteLtTriggeredWaveSource(const QString& line, const QString& kind, QStringList* warnings = nullptr);
    static QString rewriteLtBehavioralFunctions(const QString& line, QStringList* warnings = nullptr);
    static QStringList tokenizeLtOtaLine(const QString& line);
    static QString buildNgspiceOtaTranslation(const QString& line);
    static QString rewriteUnsupportedLtBehavioralTimeFunctions(const QString& line, QStringList* warnings = nullptr);
    static QString rewriteUnsupportedLtTableFunction(const QString& line, QStringList* warnings = nullptr);
    static QString buildCurrentTableExpr(const QString& xExpr, const QStringList& args, QString* error = nullptr);
    static QString rewriteUnsupportedLtStochasticFunctions(const QString& line, QStringList* warnings = nullptr);
    static QString rewriteLtBSourceLaplaceOptions(const QString& line, QStringList* warnings = nullptr);
    static QString rewriteLtBehavioralSourceRpar(const QString& line, QStringList* warnings = nullptr);
    static QString rewriteLtSourceTripOptions(const QString& line, QStringList* warnings = nullptr);
    static QString rewriteLtBSourceTripOptions(const QString& line, QStringList* warnings = nullptr);
    static void appendLtBSourceOptionWarnings(const QString& line, QStringList* warnings);
    static void appendLtSourceOptionWarnings(const QString& line, QStringList* warnings);
    static QString rewriteLtStartupSourceLine(const QString& line, QStringList* warnings = nullptr);
    static QString rewriteLtDirectiveLine(const QString& line, QStringList* warnings = nullptr, bool emulateStartup = false, const QString& projectDir = QString());
    static QStringList collapseSpiceContinuationLines(const QString& text);
    static bool rewriteLtCurrentSourceSpecial(const QString& ref, const QString& nplus, const QString& nminus, const QString& value, const QString& projectDir, QString* replacement, QStringList* warnings = nullptr);
    static QString inlinePwlFileIfNeeded(const QString& value, const QString& projectDir, QStringList* warnings = nullptr);

    // Helpers
    static QStringList splitTopLevelSpiceArgs(const QString& text);
    static int findMatchingParen(const QString& text, int openIndex);
};

#endif // SPICE_COMPAT_REWRITER_H
