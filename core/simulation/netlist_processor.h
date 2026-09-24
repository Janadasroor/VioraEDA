/*
 * Copyright 2026 Janada Sroor
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NETLIST_PROCESSOR_H
#define NETLIST_PROCESSOR_H

#include <QString>
#include <QStringList>
#include <QDir>

namespace Flux {

class NetlistProcessor {
public:
    struct Result {
        QStringList lines;
        bool success;
        QString error;
    };

    /**
     * Reads a netlist file, corrects WAV paths, comments out app-private
     * directives ngspice cannot execute (.interactive/.sp/.net), strips
     * control blocks (lifting bare analyses when the deck has none), and
     * ensures proper headers/end markers are present.
     */
    static Result process(const QString& netlistPath);

private:
    static void resolveWavPaths(QStringList& lines, const QDir& baseDir);
    static QString resolveCaseInsensitiveFilePath(const QString& path);
    static void ensureHeaderAndEnd(QStringList& lines);
    // Strips .control blocks (the shared engine cannot execute the control
    // language). Bare analysis commands found inside (tran/ac/dc/op/noise)
    // are collected into liftedAnalyses so the caller can re-inject them as
    // directives when the deck has no analysis of its own.
    static void stripControlBlocks(QStringList& lines, QStringList* liftedAnalyses = nullptr);
    static bool hasAnalysisDirective(const QStringList& lines);
    // Comments out app-private directives (.interactive/.sp/.net) that ngspice
    // reports as "unimplemented dot command" (fatal for the load). Mirrors the
    // bridge strippers; these are evaluated post-simulation by the app.
    static void commentOutAppDirectives(QStringList& lines);
};

} // namespace Flux

#endif // NETLIST_PROCESSOR_H
