/*
 * Copyright 2026 Janada Sroor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Paused-run tab-switch preservation: feed live batches for a probed
 * signal, switch scenes away and back. The signal, its data, and its
 * checked state must survive (regression: dock came back empty and the
 * run could not be followed after return).
 */

#include <QApplication>
#include <QGraphicsScene>
#include <QTemporaryDir>
#include <QtTest>

#include "../../../schematic/ui/simulation/simulation_panel.h"
#include "../../../schematic/analysis/net_manager.h"
#include "../../../ui/waveform_viewer.h"
#include <QMetaObject>
#include <QMetaType>

Q_DECLARE_METATYPE(std::vector<double>)
Q_DECLARE_METATYPE(std::vector<std::vector<double>>)

static bool feedBatch(SimulationPanel& panel,
                      const std::vector<double>& times,
                      const std::vector<std::vector<double>>& values,
                      const QStringList& names) {
    return QMetaObject::invokeMethod(&panel, "onRealTimeDataBatchReceived",
                                     Qt::DirectConnection,
                                     Q_ARG(std::vector<double>, times),
                                     Q_ARG(std::vector<std::vector<double>>, values),
                                     Q_ARG(QStringList, names));
}

static const WaveformViewer::SignalExport* findExport(
    const QList<WaveformViewer::SignalExport>& ex, const QString& name) {
    for (const auto& s : ex) {
        if (s.name.compare(name, Qt::CaseInsensitive) == 0) return &s;
    }
    return nullptr;
}

static QList<WaveformViewer::SignalExport> viewerSignals(SimulationPanel& panel) {
    auto* viewer = panel.findChild<WaveformViewer*>();
    Q_ASSERT(viewer);
    return viewer->exportSignals();
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    qRegisterMetaType<std::vector<double>>("std::vector<double>");
    qRegisterMetaType<std::vector<std::vector<double>>>("std::vector<std::vector<double>>");
    QTemporaryDir tmp;
    QGraphicsScene sceneA, sceneB;
    NetManager netA, netB;

    SimulationPanel panel(&sceneA, &netA, tmp.path());
    panel.show();
    QTest::qWait(200);

    panel.addProbe("V(OUT)");

    const std::vector<double> times = {0.0, 0.001, 0.002, 0.003, 0.004};
    std::vector<std::vector<double>> values;
    for (double t : times) values.push_back({t, 5.0 * t});
    const QStringList names = {"time", "V(OUT)"};
    if (!feedBatch(panel, times, values, names)) {
        fprintf(stderr, "RESULT ok=0 (invoke failed)\n");
        return 2;
    }
    QTest::qWait(200);

    const auto before = viewerSignals(panel);
    const auto* sigBefore = findExport(before, "V(OUT)");
    fprintf(stderr, "BEFORE-SWITCH: signals=%d found=%d points=%d checked=%d\n",
            static_cast<int>(before.size()), sigBefore ? 1 : 0,
            sigBefore ? static_cast<int>(sigBefore->time.size()) : -1,
            (sigBefore && sigBefore->checked) ? 1 : 0);
    if (!sigBefore || sigBefore->time.size() != 5 || !sigBefore->checked) {
        fprintf(stderr, "RESULT ok=0 (setup failed)\n");
        return 2;
    }

    panel.setTargetScene(&sceneB, &netB, tmp.path(), true);
    QTest::qWait(200);
    panel.setTargetScene(&sceneA, &netA, tmp.path(), true);
    QTest::qWait(200);

    const auto after = viewerSignals(panel);
    const auto* sigAfter = findExport(after, "V(OUT)");
    fprintf(stderr, "AFTER-RETURN: signals=%d found=%d points=%d checked=%d\n",
            static_cast<int>(after.size()), sigAfter ? 1 : 0,
            sigAfter ? static_cast<int>(sigAfter->time.size()) : -1,
            (sigAfter && sigAfter->checked) ? 1 : 0);

    const bool ok = sigAfter && sigAfter->time.size() == 5 && sigAfter->checked;
    fprintf(stderr, "RESULT ok=%d\n", ok ? 1 : 0);
    return ok ? 0 : 2;
}
