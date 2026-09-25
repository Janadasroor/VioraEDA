/*
 * Copyright 2026 Janada Sroor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Stress test for the live-mode engine stall flakes:
 *  1. Rapid successive alter/toggle updates must not wedge the engine
 *     (each executeSequence must confirm a halt sync point before altering).
 *  2. stopSimulation() followed immediately by runSimulation() must not let a
 *     stale queued bg_halt strand the new run in Halted.
 * Failure mode is a hang/timeout, so any exit is a pass; exit codes follow the
 * other regression tests (0 ok, 1 start/data failure, 2 stall).
 */

#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include <QSignalSpy>
#include <QTest>
#include <QThread>
#include "simulation/simulation_manager.h"
#include <iostream>

#include <QTemporaryFile>

#include "simulator/core/sim_results.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    qDebug() << "Starting Interactive Stress Test...";

    SimulationManager& sim = SimulationManager::instance();
    sim.initialize();

    QString netlist =
        "Viospice Stress Netlist\n"
        "V1 1 0 5\n"
        "RSW1 1 2 1k\n"
        "R1 2 0 1k\n"
        "C1 2 0 100u\n"
        ".tran 10u 20 0\n"
        ".end\n";

    QTemporaryFile tempNetlist;
    if (!tempNetlist.open()) {
        fprintf(stderr, "STRESS: failed to create temp netlist\n");
        return 1;
    }
    tempNetlist.write(netlist.toUtf8());
    tempNetlist.flush();
    tempNetlist.close();

    SimControl ctrl;
    QSignalSpy startSpy(&sim, &SimulationManager::simulationStarted);
    QSignalSpy dataSpy(&sim, &SimulationManager::realTimeDataBatchReceived);
    QSignalSpy errorSpy(&sim, &SimulationManager::errorOccurred);

    // --- Phase 1: run and wait for streaming data ---
    sim.runSimulation(tempNetlist.fileName(), &ctrl);
    if (startSpy.count() == 0 && !startSpy.wait(5000)) {
        fprintf(stderr, "STRESS: phase1 failed to start\n");
        return 1;
    }
    if (!dataSpy.wait(5000)) {
        fprintf(stderr, "STRESS: phase1 no streaming data\n");
        return 1;
    }

    // --- Phase 2: rapid toggles with no pacing ---
    fprintf(stderr, "STRESS: issuing 8 rapid toggles...\n");
    for (int i = 0; i < 8; ++i) {
        sim.alterSwitchResistance("RSW1", (i % 2 == 0) ? 500.0 : 2000.0);
        sim.alterSwitchVoltage("V1", (i % 2 == 0) ? 5.0 : 3.3);
    }

    int batches = 0;
    for (int i = 0; i < 100 && batches < 10; ++i) {
        if (dataSpy.wait(100)) {
            auto args = dataSpy.last();
            auto times = args.at(0).value<std::vector<double>>();
            if (!times.empty()) batches++;
        }
    }
    if (batches < 10) {
        fprintf(stderr, "STRESS: FAIL - stall after rapid toggles (batches=%d) state=%s isRunning=%d\n",
                batches, sim.stateString().toUtf8().constData(), sim.isRunning() ? 1 : 0);
        sim.shutdown();
        return 2;
    }
    fprintf(stderr, "STRESS: phase2 ok (%d batches after toggles)\n", batches);

    // --- Phase 2b: alters must take ELECTRICAL effect, not just not-stall ---
    // V1=5V with RSW1=500 on the V1-RSW1-R1(1k) divider puts V(2) at 3.33V.
    // Regression: with the halt skipped, the engine rejected every alter
    // ("type bg_halt first") and values never moved.
    dataSpy.clear();
    sim.alterSwitchVoltage("V1", 5.0);
    sim.alterSwitchResistance("RSW1", 500.0);
    QTest::qWait(1500); // halt/alter/resume latency + RC settle (tau ~33ms)
    dataSpy.clear(); // drop anything queued before the alter landed
    bool inRange = false;
    double v2last = 0.0;
    for (int i = 0; i < 30 && !inRange; ++i) {
        if (!dataSpy.wait(200)) break; // run may have finished; fail below
        const auto args = dataSpy.last();
        const auto times = args.at(0).value<std::vector<double>>();
        const auto rows = args.at(1).value<std::vector<std::vector<double>>>();
        const auto names = args.at(2).value<QStringList>();
        int idx = -1;
        for (int n = 0; n < names.size(); ++n) {
            if (names.at(n).endsWith("(2)", Qt::CaseInsensitive)) { idx = n; break; }
        }
        if (idx < 0 || times.empty() || rows.empty()) continue;
        const auto& lastRow = rows.back();
        if ((int)lastRow.size() <= idx) continue;
        v2last = lastRow[idx];
        if (v2last > 2.5 && v2last < 4.2) inRange = true;
    }
    fprintf(stderr, "STRESS: alter-effect v2=%g inRange=%d\n", v2last, inRange ? 1 : 0);
    if (!inRange) {
        fprintf(stderr, "STRESS: FAIL - alter had no electrical effect\n");
        sim.shutdown();
        return 2;
    }

    // --- Phase 3: stop then IMMEDIATELY re-run (no wait for async cleanup) ---
    fprintf(stderr, "STRESS: stop + immediate rerun... (state=%s)\n", sim.stateString().toUtf8().constData());
    const int startCount = startSpy.count();
    sim.stopSimulation();
    sim.runSimulation(tempNetlist.fileName(), &ctrl);
    fprintf(stderr, "STRESS: post-run state=%s lastErr=%s starts=%d\n",
            sim.stateString().toUtf8().constData(),
            sim.lastErrorMessage().toUtf8().constData(), startSpy.count());

    bool secondStarted = false;
    for (int i = 0; i < 50 && !secondStarted; ++i) {
        if (startSpy.count() > startCount) { secondStarted = true; break; }
        startSpy.wait(100);
    }

    bool secondData = false;
    if (secondStarted) {
        for (int i = 0; i < 50 && !secondData; ++i) {
            if (dataSpy.wait(100)) {
                auto args = dataSpy.last();
                auto times = args.at(0).value<std::vector<double>>();
                if (!times.empty()) secondData = true;
            }
        }
    }

    const int errs = errorSpy.count();
    fprintf(stderr, "STRESS: secondStarted=%d secondData=%d errors=%d\n",
            secondStarted ? 1 : 0, secondData ? 1 : 0, errs);

    if (!secondStarted || !secondData) {
        fprintf(stderr, "STRESS: FAIL - second run did not proceed (stale halt?)\n");
        sim.shutdown();
        return 2;
    }

    QSignalSpy finishSpy(&sim, &SimulationManager::simulationFinished);
    sim.stopSimulation();
    finishSpy.wait(2000);
    fprintf(stderr, "STRESS: SUCCESS\n");
    return 0;
}
