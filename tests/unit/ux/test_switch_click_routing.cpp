/*
 * Copyright 2026 Janada Sroor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Switch click routing: with a simulation running, clicking an interactive
 * switch must toggle it — not arm a probe. Regression: the press handler
 * routed through findProbeableComponentAt, which excludes interactive
 * components, so with a net under the cursor the click fell into the probe
 * flow (probe armed, event accepted) and the switch could never be
 * opened/closed while simulating.
 */

#include <QApplication>
#include <QGraphicsScene>
#include <QtTest>

#include "../../../schematic/editor/schematic_view.h"
#include "../../../schematic/items/switch_item.h"
#include "../../../schematic/items/wire_item.h"
#include "../../../schematic/analysis/net_manager.h"
#include "../../../schematic/tools/schematic_tool.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    // Declared before the view: ~SchematicView touches the current tool.
    SchematicTool selectTool(QStringLiteral("Select"));

    QGraphicsScene scene;
    NetManager netMgr;
    SwitchItem* sw = new SwitchItem(QPointF(0, 0));
    scene.addItem(sw);
    // A live net under the cursor: this is what diverted the click into the
    // probe flow on the buggy code (no net => fallback toggle path).
    WireItem* wire = new WireItem(QPointF(-100, 0), QPointF(100, 0));
    scene.addItem(wire);
    netMgr.updateNets(&scene);
    if (!sw->isOpen()) {
        fprintf(stderr, "RESULT ok=0 (switch should start open)\n");
        return 2;
    }

    SchematicView view;
    view.setScene(&scene);
    view.setNetManager(&netMgr);
    // The tool registry needs builtin resources unavailable in tests; a stub
    // Select tool exercises the same press-handler branch.
    view.setCurrentTool(&selectTool);
    // Bug condition: simulation running => probe flow is armed.
    view.setSimulationRunning(true);
    view.resize(400, 300);
    view.show();
    QTest::qWait(200);

    const QPoint viewPos = view.mapFromScene(sw->pos());
    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, viewPos);
    QTest::qWait(100);

    fprintf(stderr, "RESULT open=%d (want 0 after one click)\n", sw->isOpen() ? 1 : 0);
    if (sw->isOpen()) {
        fprintf(stderr, "RESULT ok=0 (click did not toggle the switch)\n");
        return 1;
    }

    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, viewPos);
    QTest::qWait(100);
    fprintf(stderr, "RESULT open=%d (want 1 after two clicks)\n", sw->isOpen() ? 1 : 0);
    if (!sw->isOpen()) {
        fprintf(stderr, "RESULT ok=0 (second click did not toggle back)\n");
        return 1;
    }

    fprintf(stderr, "RESULT ok=1\n");
    return 0;
}
