/*
 * Copyright 2026 Janada Sroor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Reproduces pause/resume/stop result delivery on the shared engine:
 * run a transient, halt it mid-run, resume to completion (raw must be
 * written, no errors), then run again and stop mid-run (partial raw must
 * still be written, no errors).
 */

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTimer>
#include <QElapsedTimer>

#include "simulation_manager.h"

static bool waitState(SimulationManager& sim, SimulationState want, int timeoutMs) {
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < timeoutMs) {
        if (sim.state() == want) return true;
        QEventLoop tick;
        QTimer::singleShot(50, &tick, &QEventLoop::quit);
        tick.exec();
    }
    return sim.state() == want;
}

static int waitFinish(SimulationManager& sim, int timeoutMs) {
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    int finished = 0;
    QObject::connect(&sim, qOverload<>(&SimulationManager::simulationFinished), &loop, [&]() { ++finished; loop.quit(); });
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();
    return finished;
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    auto& sim = SimulationManager::instance();
    if (!sim.isAvailable()) {
        fprintf(stderr, "Ngspice not available\n");
        return 1;
    }
    sim.initialize();

    const QString deckPath = QDir::tempPath() + "/pause_resume_raw.cir";
    {
        QFile f(deckPath);
        if (f.open(QIODevice::WriteOnly)) {
            f.write("Pause resume test\nV1 1 0 10\nR1 1 2 1k\nC1 2 0 1u\n.tran 1u 20m\n.end\n");
            f.close();
        }
    }
    const QString rawPath = QDir::tempPath() + "/pause_resume_raw.raw";
    QFile::remove(rawPath);

    QSignalSpy errorSpy(&sim, &SimulationManager::errorOccurred);
    QSignalSpy rawSpy(&sim, &SimulationManager::rawResultsReady);

    bool ok = true;

    // Phase A: run -> halt -> resume -> finish with raw, no errors.
    sim.runSimulation(deckPath, nullptr);
    if (!waitState(sim, SimulationState::Running, 15000)) {
        fprintf(stderr, "PHASE-A: never reached Running (state=%d)\n", static_cast<int>(sim.state()));
        ok = false;
    } else {
        // Let the ngspice bg thread spin up and produce points first: a
        // bg_halt issued before bg_run is dispatched is silently lost.
        QEventLoop spin;
        QTimer::singleShot(3000, &spin, &QEventLoop::quit);
        spin.exec();
        sim.sendInternalCommand("bg_halt");
        if (!waitState(sim, SimulationState::Halted, 10000)) {
            fprintf(stderr, "PHASE-A: bg_halt did not park (state=%d)\n", static_cast<int>(sim.state()));
            ok = false;
        } else {
            sim.sendInternalCommand("bg_resume");
            if (!waitState(sim, SimulationState::Running, 10000)) {
                fprintf(stderr, "PHASE-A: bg_resume did not resume (state=%d)\n", static_cast<int>(sim.state()));
                ok = false;
            }
        }
    }
    const int finishedA = waitFinish(sim, 150000);
    const int rawsA = rawSpy.count();
    const qint64 sizeA = QFileInfo(rawPath).exists() ? QFileInfo(rawPath).size() : 0;
    fprintf(stderr, "PHASE-A: finished=%d rawResults=%d rawBytes=%lld errors=%d\n",
            finishedA, rawsA, static_cast<long long>(sizeA), static_cast<int>(errorSpy.count()));
    if (finishedA < 1 || rawsA < 1 || sizeA <= 0) ok = false;

    // Phase B: run -> stop mid-run -> partial raw still written, no errors.
    const int errsBefore = errorSpy.count();
    const int rawsBefore = rawSpy.count();
    QFile::remove(rawPath);
    sim.runSimulation(deckPath, nullptr);
    if (!waitState(sim, SimulationState::Running, 15000)) {
        fprintf(stderr, "PHASE-B: never reached Running\n");
        ok = false;
    } else {
        QEventLoop tick;
        QTimer::singleShot(3000, &tick, &QEventLoop::quit);
        tick.exec();
        sim.stopSimulation();
    }
    waitFinish(sim, 60000);
    const int rawsB = rawSpy.count() - rawsBefore;
    const qint64 sizeB = QFileInfo(rawPath).exists() ? QFileInfo(rawPath).size() : 0;
    fprintf(stderr, "PHASE-B: rawResults=%d rawBytes=%lld errors=%d\n",
            rawsB, static_cast<long long>(sizeB), static_cast<int>(errorSpy.count() - errsBefore));
    if (rawsB < 1 || sizeB <= 0) ok = false;

    const int totalErrs = errorSpy.count();
    for (int i = 0; i < totalErrs; ++i) {
        fprintf(stderr, "ERROR[%d]=%s\n", i, errorSpy.at(i).at(0).toString().toUtf8().constData());
    }
    if (totalErrs > 0) ok = false;

    QFile::remove(deckPath);
    QFile::remove(rawPath);
    fprintf(stderr, "RESULT ok=%d\n", ok ? 1 : 0);
    return ok ? 0 : 2;
}
