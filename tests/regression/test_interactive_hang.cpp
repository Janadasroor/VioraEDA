#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include <QSignalSpy>
#include "simulation/simulation_manager.h"
#include "simulation/spice_backend.h"
#include <iostream>

#include <QTemporaryFile>

#include "simulator/core/sim_results.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    qDebug() << "Starting Interactive Hang Test...";
    
    SimulationManager& sim = SimulationManager::instance();
    sim.initialize();
    
    // Simple circuit with a switch (resistor-based)
    QString netlist = 
        "Viospice Test Netlist\n"
        "V1 1 0 5\n"
        "RSW1 1 2 1k\n"
        "R1 2 0 1k\n"
        ".tran 100u 10 0\n"
        ".end\n";
        
    QTemporaryFile tempNetlist;
    if (!tempNetlist.open()) {
        qCritical() << "Failed to create temp netlist";
        return 1;
    }
    tempNetlist.write(netlist.toUtf8());
    tempNetlist.flush();
    tempNetlist.close();

    SimControl ctrl;
    QSignalSpy startSpy(&sim, &SimulationManager::simulationStarted);
    QSignalSpy finishSpy(&sim, &SimulationManager::simulationFinished);
    QSignalSpy dataSpy(&sim, &SimulationManager::realTimeDataBatchReceived);
    QSignalSpy errorSpy(&sim, &SimulationManager::errorOccurred);

    qDebug() << "Running simulation on:" << tempNetlist.fileName();
    sim.runSimulation(tempNetlist.fileName(), &ctrl);
    
    if (startSpy.count() == 0 && !startSpy.wait(5000)) {
        QString err = errorSpy.count() > 0 ? errorSpy.first().at(0).toString() : "Unknown";
        qCritical() << "Simulation failed to start! Error:" << err;
        return 1;
    }
    
    qDebug() << "Simulation started. Waiting for initial data...";
    
    // Wait for at least one data batch
    if (!dataSpy.wait(5000)) {
        qCritical() << "Timeout waiting for data!";
        return 1;
    }
    
    qDebug() << "Received data. Toggling switch...";
    
    // Toggle switch: change 1k to 500
    sim.alterSwitchResistance("RSW1", 500.0);
    
    qDebug() << "Switch toggle issued. Waiting for more data...";
    
    // Now wait for more data to arrive AFTER the toggle
    int validBatches = 0;
    int postDataCount = 0;
    for (int i = 0; i < 200; ++i) { // Increased from 40 to 200
        if (dataSpy.wait(100)) { // Shorter wait per iteration
            auto signalArgs = dataSpy.last();
            std::vector<double> times = signalArgs.at(0).value<std::vector<double>>();
            if (!times.empty()) {
                validBatches++;
                qDebug() << "Received valid data batch after toggle (" << validBatches << "/5). Points:" << times.size();
                if (validBatches >= 5) break;
            } else {
                qDebug() << "Received empty data batch. Iteration:" << i;
            }
        }
    }
    
    if (validBatches < 5) {
        qCritical() << "FAILURE: Hang detected! Only received" << validBatches << "valid batches after toggle.";
        return 2;
    }
    
    qDebug() << "SUCCESS: Data continues to flow after switch toggle.";
    
    // Wait for simulation to finish
    if (!finishSpy.wait(2000)) {
        qDebug() << "Stopping simulation...";
        sim.stopSimulation();
        finishSpy.wait(1000);
    }
    
    qDebug() << "Test completed successfully.";
    return 0;
}
