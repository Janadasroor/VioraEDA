# FluxScript Integration with vio-cmd CLI

## Overview

This document describes how to integrate FluxScript with the existing VioSpice `vio-cmd` CLI tool.

## Architecture

```
vio-cmd CLI (VioSpice)
    │
    ├── sim command (existing)
    │   └── ngspice simulation
    │
    ├── flux command (NEW)
    │   ├── flux run     - Run FluxScript file
    │   ├── flux compile - Compile to netlist
    │   ├── flux eval    - Evaluate expression
    │   └── flux repl    - Interactive REPL
    │
    └── netlist command (existing)
        └── SPICE netlist generation
```

## Implementation

### Step 1: Add FluxScript Command Handler

Create `cli/flux_command.cpp`:

```cpp
#include "flux_command.h"
#include <flux/jit_engine.h>
#include <flux/compiler/compiler_instance.h>
#include <flux/runtime/ngspice_bridge.h>

#include <QFile>
#include <QTextStream>
#include <QCommandLineParser>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>
#include <iostream>

namespace VioSpice {

FluxCommand::FluxCommand(QObject* parent) : QObject(parent) {}

int FluxCommand::run(const QStringList& args, bool quiet) {
    QCommandLineParser parser;
    parser.setApplicationDescription("FluxScript SPICE Integration");
    
    parser.addPositionalArgument("subcommand", 
        "Subcommand: run, compile, eval, repl", "<subcommand>");
    parser.addPositionalArgument("file", 
        "FluxScript file to process", "[file.flux]");
    
    QCommandLineOption outputOption(QStringList() << "o" << "output",
        "Output file for results", "file");
    parser.addOption(outputOption);
    
    QCommandLineOption netlistOption("netlist",
        "Generate SPICE netlist only");
    parser.addOption(netlistOption);
    
    QCommandLineOption runOption("run",
        "Run simulation after compilation");
    parser.addOption(runOption);
    
    QCommandLineOption tranOption("tran",
        "Transient analysis parameters (tstep,tstop)", "params");
    parser.addOption(tranOption);
    
    QCommandLineOption jsonOption("json",
        "Output results as JSON");
    parser.addOption(jsonOption);
    
    parser.addHelpOption();
    
    if (!parser.parse(args)) {
        std::cerr << qPrintable(parser.errorText()) << std::endl;
        return 1;
    }
    
    const QStringList positional = parser.positionalArguments();
    if (positional.isEmpty()) {
        printHelp();
        return 0;
    }
    
    const QString subcommand = positional[0];
    
    if (subcommand == "run") {
        return cmdRun(parser, positional, quiet);
    } else if (subcommand == "compile") {
        return cmdCompile(parser, positional, quiet);
    } else if (subcommand == "eval") {
        return cmdEval(parser, positional, quiet);
    } else if (subcommand == "repl") {
        return cmdRepl(parser, positional, quiet);
    } else {
        std::cerr << "Unknown subcommand: " << qPrintable(subcommand) << std::endl;
        printHelp();
        return 1;
    }
}

int FluxCommand::cmdRun(const QCommandLineParser& parser, 
                        const QStringList& positional, 
                        bool quiet) {
    if (positional.size() < 2) {
        std::cerr << "Error: Missing file argument" << std::endl;
        return 1;
    }
    
    const QString inputFile = positional[1];
    
    // Read input file
    QFile file(inputFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::cerr << "Error: Cannot open file: " 
                  << qPrintable(inputFile) << std::endl;
        return 1;
    }
    
    QTextStream in(&file);
    QString code = in.readAll();
    file.close();
    
    if (!quiet) {
        std::cout << "FluxScript: Compiling " 
                  << qPrintable(inputFile) << "..." << std::endl;
    }
    
    // Initialize JIT
    Flux::JITEngine::instance().initialize();
    
    // Compile script
    std::string error;
    if (!Flux::JITEngine::instance().compileScript(code.toStdString(), &error)) {
        std::cerr << "Compilation Error: " << error << std::endl;
        return 1;
    }
    
    if (!quiet) {
        std::cout << "✓ Compilation successful" << std::endl;
    }
    
    // Check if netlist generation only
    if (parser.isSet("netlist")) {
        return generateNetlist(code, inputFile, parser.value("output"), quiet);
    }
    
    // Run simulation
    if (parser.isSet("run")) {
        return runSimulation(inputFile, parser.value("output"), 
                           parser.value("tran"), quiet);
    }
    
    return 0;
}

int FluxCommand::cmdCompile(const QCommandLineParser& parser,
                            const QStringList& positional,
                            bool quiet) {
    if (positional.size() < 2) {
        std::cerr << "Error: Missing file argument" << std::endl;
        return 1;
    }
    
    const QString inputFile = positional[1];
    QString outputFile = parser.value("output");
    
    if (outputFile.isEmpty()) {
        outputFile = inputFile + ".cir";
    }
    
    // Read and compile
    QFile file(inputFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::cerr << "Error: Cannot open file: " 
                  << qPrintable(inputFile) << std::endl;
        return 1;
    }
    
    QTextStream in(&file);
    QString code = in.readAll();
    file.close();
    
    // Generate netlist
    return generateNetlist(code, inputFile, outputFile, quiet);
}

int FluxCommand::cmdEval(const QCommandLineParser& parser,
                         const QStringList& positional,
                         bool quiet) {
    if (positional.size() < 2) {
        std::cerr << "Error: Missing expression argument" << std::endl;
        return 1;
    }
    
    const QString expression = positional[1];
    
    // Initialize JIT
    Flux::JITEngine::instance().initialize();
    
    // Create simple script
    QString code = expression;
    
    std::string error;
    if (!Flux::JITEngine::instance().compileScript(code.toStdString(), &error)) {
        std::cerr << "Compilation Error: " << error << std::endl;
        return 1;
    }
    
    // Execute and get result
    auto result = Flux::JITEngine::instance().callFunction(
        "__anon_expr", {}, &error);
    
    if (!error.empty()) {
        std::cerr << "Execution Error: " << error << std::endl;
        return 1;
    }
    
    // Print result
    if (parser.isSet("json")) {
        QJsonObject json;
        if (std::holds_alternative<double>(result)) {
            json["value"] = std::get<double>(result);
            json["type"] = "double";
        } else if (std::holds_alternative<int>(result)) {
            json["value"] = std::get<int>(result);
            json["type"] = "int";
        }
        QJsonDocument doc(json);
        std::cout << doc.toJson(QJsonDocument::Compact).constData() << std::endl;
    } else {
        if (std::holds_alternative<double>(result)) {
            std::cout << std::get<double>(result) << std::endl;
        } else if (std::holds_alternative<int>(result)) {
            std::cout << std::get<int>(result) << std::endl;
        }
    }
    
    return 0;
}

int FluxCommand::cmdRepl(const QCommandLineParser& parser,
                         const QStringList& positional,
                         bool quiet) {
    (void)parser;
    (void)positional;
    
    std::cout << "FluxScript REPL (Read-Eval-Print Loop)" << std::endl;
    std::cout << "Type 'exit' or Ctrl+D to quit" << std::endl;
    std::cout << std::endl;
    
    Flux::JITEngine::instance().initialize();
    
    QString line;
    int counter = 0;
    
    while (true) {
        std::cout << "flux[" << counter++ << "]> " << std::flush;
        
        if (!std::getline(std::cin, line)) {
            break;
        }
        
        line = line.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        
        if (line == "exit" || line == "quit") {
            break;
        }
        
        if (line == "help") {
            printReplHelp();
            continue;
        }
        
        // Compile and execute
        std::string error;
        if (!Flux::JITEngine::instance().compileScript(line.toStdString(), &error)) {
            std::cerr << "Error: " << error << std::endl;
            continue;
        }
        
        auto result = Flux::JITEngine::instance().callFunction(
            "__anon_expr", {}, &error);
        
        if (!error.empty()) {
            std::cerr << "Error: " << error << std::endl;
            continue;
        }
        
        if (std::holds_alternative<double>(result)) {
            std::cout << "= " << std::get<double>(result) << std::endl;
        } else if (std::holds_alternative<int>(result)) {
            std::cout << "= " << std::get<int>(result) << std::endl;
        }
    }
    
    return 0;
}

int FluxCommand::generateNetlist(const QString& code,
                                  const QString& inputFile,
                                  const QString& outputFile,
                                  bool quiet) {
    // Simplified netlist generation
    // Full implementation would use NetlistGenerator class
    
    QString netlist;
    netlist += "* FluxScript Generated Netlist\n";
    netlist += "* Source: " + inputFile + "\n";
    netlist += "* Generated by vio-cmd flux\n";
    netlist += "\n";
    netlist += ".PARAM R=1k C=1u VCC=5\n";
    netlist += ".IC V(out)=0\n";
    netlist += ".TRAN 1u 1m\n";
    netlist += ".PROBE V(out) V(in)\n";
    netlist += ".END\n";
    
    QFile outFile(outputFile);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        std::cerr << "Error: Cannot write to " 
                  << qPrintable(outputFile) << std::endl;
        return 1;
    }
    
    QTextStream out(&outFile);
    out << netlist;
    outFile.close();
    
    if (!quiet) {
        std::cout << "✓ Generated netlist: " 
                  << qPrintable(outputFile) << std::endl;
    }
    
    return 0;
}

int FluxCommand::runSimulation(const QString& inputFile,
                                const QString& outputFile,
                                const QString& tranParams,
                                bool quiet) {
    (void)inputFile;
    
    if (!quiet) {
        std::cout << "Running ngspice simulation..." << std::endl;
    }
    
    // Parse transient parameters
    double tStep = 1e-6;
    double tStop = 1e-3;
    
    if (!tranParams.isEmpty()) {
        QStringList parts = tranParams.split(",");
        if (parts.size() >= 2) {
            tStep = parts[0].toDouble();
            tStop = parts[1].toDouble();
        }
    }
    
    // Initialize ngspice bridge (stub)
    int result = Flux::flux_ngspice_init(nullptr);
    if (result != 0) {
        if (!quiet) {
            std::cout << "Note: Running in stub mode (ngspice not linked)" << std::endl;
        }
    }
    
    // Run transient simulation
    result = Flux::flux_ngspice_run_transient(0.0, tStop, tStep);
    if (result != 0) {
        std::cerr << "Error: Simulation failed" << std::endl;
        Flux::flux_ngspice_cleanup();
        return 1;
    }
    
    if (!quiet) {
        std::cout << "✓ Simulation completed" << std::endl;
    }
    
    // Write results
    if (!outputFile.isEmpty()) {
        QFile outFile(outputFile);
        if (outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&outFile);
            out << "time,v(out)\n";
            
            int vecSize = Flux::flux_ngspice_get_vector_size("v(out)");
            for (int i = 0; i < qMin(vecSize, 1000); ++i) {
                double time = Flux::flux_get_time();
                double vout = Flux::flux_ngspice_get_vector("v(out)", i);
                out << time << "," << vout << "\n";
            }
            
            outFile.close();
            
            if (!quiet) {
                std::cout << "✓ Results written to: " 
                          << qPrintable(outputFile) << std::endl;
            }
        }
    }
    
    Flux::flux_ngspice_cleanup();
    return 0;
}

void FluxCommand::printHelp() {
    std::cout << "vio-cmd flux - FluxScript SPICE Integration\n";
    std::cout << "\n";
    std::cout << "Usage: vio-cmd flux <subcommand> [options] [file.flux]\n";
    std::cout << "\n";
    std::cout << "Subcommands:\n";
    std::cout << "  run <file.flux>     Compile and run FluxScript file\n";
    std::cout << "  compile <file.flux> Generate SPICE netlist\n";
    std::cout << "  eval <expression>   Evaluate FluxScript expression\n";
    std::cout << "  repl                Interactive REPL mode\n";
    std::cout << "\n";
    std::cout << "Options:\n";
    std::cout << "  -o, --output <file>  Output file for results\n";
    std::cout << "  --netlist           Generate netlist only\n";
    std::cout << "  --run               Run simulation\n";
    std::cout << "  --tran <tstep,tstop> Transient parameters\n";
    std::cout << "  --json              Output as JSON\n";
    std::cout << "  -q, --quiet         Quiet mode\n";
    std::cout << "  -h, --help          Show help\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  vio-cmd flux run circuit.flux\n";
    std::cout << "  vio-cmd flux compile circuit.flux -o circuit.cir\n";
    std::cout << "  vio-cmd flux eval \"sin(pi/2)\"\n";
    std::cout << "  vio-cmd flux repl\n";
}

void FluxCommand::printReplHelp() {
    std::cout << "FluxScript REPL Commands:\n";
    std::cout << "  help              Show this help\n";
    std::cout << "  exit, quit        Exit REPL\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  > 2 + 2\n";
    std::cout << "  > def f(x) x * x; f(5)\n";
    std::cout << "  > param R = 1k; R\n";
    std::cout << "  > time\n";
    std::cout << "  > state_get(\"x\", 0)\n";
}

} // namespace VioSpice
```

### Step 2: Create Header File

Create `cli/flux_command.h`:

```cpp
#ifndef VIOSPICE_FLUX_COMMAND_H
#define VIOSPICE_FLUX_COMMAND_H

#include <QObject>
#include <QStringList>

class QCommandLineParser;

namespace VioSpice {

/**
 * @brief FluxScript integration command for vio-cmd CLI
 */
class FluxCommand : public QObject {
    Q_OBJECT
    
public:
    explicit FluxCommand(QObject* parent = nullptr);
    
    /**
     * @brief Run flux subcommand
     * @param args Command line arguments
     * @param quiet Quiet mode flag
     * @return Exit code (0 = success)
     */
    int run(const QStringList& args, bool quiet = false);

private:
    // Subcommand handlers
    int cmdRun(const QCommandLineParser& parser, 
               const QStringList& positional,
               bool quiet);
    int cmdCompile(const QCommandLineParser& parser,
                   const QStringList& positional,
                   bool quiet);
    int cmdEval(const QCommandLineParser& parser,
                const QStringList& positional,
                bool quiet);
    int cmdRepl(const QCommandLineParser& parser,
                const QStringList& positional,
                bool quiet);
    
    // Helper functions
    int generateNetlist(const QString& code,
                        const QString& inputFile,
                        const QString& outputFile,
                        bool quiet);
    int runSimulation(const QString& inputFile,
                      const QString& outputFile,
                      const QString& tranParams,
                      bool quiet);
    
    void printHelp();
    void printReplHelp();
};

} // namespace VioSpice

#endif // VIOSPICE_FLUX_COMMAND_H
```

### Step 3: Integrate with vio-cmd main.cpp

Add to `cli/main.cpp`:

```cpp
#include "flux_command.h"

// In main() function, add flux subcommand handling:

if (argc > 1 && QString(argv[1]) == "flux") {
    VioSpice::FluxCommand fluxCmd;
    QStringList fluxArgs;
    for (int i = 2; i < argc; ++i) {
        fluxArgs << QString(argv[i]);
    }
    return fluxCmd.run(fluxArgs, g_quiet);
}
```

## Usage Examples

### Run FluxScript File
```bash
vio-cmd flux run examples/test_spice_integration.flux
```

### Generate Netlist
```bash
vio-cmd flux compile circuit.flux -o circuit.cir
```

### Evaluate Expression
```bash
vio-cmd flux eval "sin(pi/2)"
# Output: 1
```

### Interactive REPL
```bash
vio-cmd flux repl
# flux[0]> 2 + 2
# = 4
# flux[1]> def f(x) x * x; f(5)
# = 25
# flux[2]> exit
```

### Run Simulation
```bash
vio-cmd flux run circuit.flux --run --output results.csv --tran 1u,1m
```

## Build Instructions

1. Add new files to CMakeLists.txt:
```cmake
# In viospice/cli/CMakeLists.txt
set(VIO_CMD_SOURCES
    ${VIO_CMD_SOURCES}
    flux_command.cpp
)
```

2. Link with FluxScript library:
```cmake
target_link_libraries(vio-cmd PRIVATE
    FluxScript
    ${NGSPICE_LIBRARIES}
)
```

3. Include FluxScript headers:
```cmake
target_include_directories(vio-cmd PRIVATE
    ${FLUXSCRIPT_INCLUDE_DIR}
)
```

## Testing

```bash
# Test help
vio-cmd flux --help

# Test REPL
echo -e "2+2\nexit" | vio-cmd flux repl

# Test eval
vio-cmd flux eval "param R = 1k; R"

# Test compile
vio-cmd flux compile ../fluxscript/examples/test_spice_integration.flux
```
