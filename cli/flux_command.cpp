#include "flux_command.h"

#ifdef HAVE_FLUXSCRIPT
#include <flux/jit_engine.h>
#include <flux/compiler/compiler_instance.h>
#include <flux/compiler/netlist_generator.h>
#include <flux/runtime/ngspice_bridge.h>
#endif

#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QTextStream>
#include <QCommandLineParser>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>
#include <iostream>
#include <variant>

namespace VioSpice {

FluxCommand::FluxCommand(QObject* parent) : QObject(parent) {}

int FluxCommand::run(const QStringList& args, const QCommandLineParser& globalParser, bool quiet) {
    QCommandLineParser parser;
    parser.setApplicationDescription("FluxScript SPICE Integration");
    
    // Add a dummy executable name so the first real argument isn't skipped
    QStringList argsWithExec;
    argsWithExec << "viora flux" << args;
    
    parser.addPositionalArgument("subcommand", 
        "Subcommand: run, compile, eval, repl", "<subcommand>");
    parser.addPositionalArgument("file", 
        "FluxScript file to process", "[file.flux]");
    
    // We still define 'output' locally for when called standalone or with -- (not implemented yet)
    // but we will prioritize globalParser.value("out") or globalParser.value("output")
    QCommandLineOption outputOption(QStringList() << "output",
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

    QCommandLineOption timeOption("time",
        "Time value for template execution", "value");
    parser.addOption(timeOption);

    QCommandLineOption inputsOption("inputs",
        "Input values for template (comma separated)", "values");
    parser.addOption(inputsOption);
    
    parser.addHelpOption();
    
    if (!parser.parse(argsWithExec)) {
        std::cerr << qPrintable(parser.errorText()) << std::endl;
        return 1;
    }
    
    // Merge global options if not set locally
    QString outputFile = parser.value("output");
    if (outputFile.isEmpty()) {
        outputFile = globalParser.value("out");
    }
    if (outputFile.isEmpty()) {
        outputFile = globalParser.value("output");
    }
    
    const QStringList positional = parser.positionalArguments();
    if (positional.isEmpty()) {
        printHelp();
        return 0;
    }
    
    const QString subcommand = positional[0];
    
    if (subcommand == "run") {
        return cmdRun(parser, positional, outputFile, quiet);
    } else if (subcommand == "compile") {
        return cmdCompile(parser, positional, outputFile, quiet);
    } else if (subcommand == "eval") {
        return cmdEval(parser, positional, quiet);
    } else if (subcommand == "repl") {
        return cmdRepl(parser, positional, quiet);
    } else if (subcommand == "template-list") {
        return cmdTemplateList(quiet);
    } else if (subcommand == "template-run") {
        return cmdTemplateRun(parser, globalParser, positional, outputFile, quiet);
    } else {
        std::cerr << "Unknown subcommand: " << qPrintable(subcommand) << std::endl;
        printHelp();
        return 1;
    }
}

int FluxCommand::cmdTemplateList(bool quiet) {
    QString appPath = QCoreApplication::applicationDirPath();
    QString templatesPath = QDir(appPath).absoluteFilePath("../python/templates");
    if (!QFile::exists(templatesPath)) {
        templatesPath = QDir(appPath).absoluteFilePath("python/templates");
    }

    if (!QDir(templatesPath).exists()) {
        std::cerr << "Error: Templates directory not found at " << qPrintable(templatesPath) << std::endl;
        return 1;
    }

    QDir dir(templatesPath);
    QStringList files = dir.entryList({"*.flux"}, QDir::Files);
    
    if (!quiet) {
        std::cout << "Available FluxScript Templates:" << std::endl;
        for (const QString& file : files) {
            QString name = file.section('.', 0, 0);
            std::cout << "  - " << qPrintable(name) << std::endl;
        }
    } else {
        for (const QString& file : files) {
            std::cout << qPrintable(file.section('.', 0, 0)) << std::endl;
        }
    }
    
    return 0;
}

int FluxCommand::cmdTemplateRun(const QCommandLineParser& parser,
                               const QCommandLineParser& globalParser,
                               const QStringList& positional,
                               const QString& outputFile,
                               bool quiet) {
    if (positional.size() < 2) {
        std::cerr << "Error: Missing template name" << std::endl;
        return 1;
    }
    
    QString templateName = positional[1];
    if (!templateName.endsWith(".flux")) {
        templateName += ".flux";
    }

    QString appPath = QCoreApplication::applicationDirPath();
    QString templatesPath = QDir(appPath).absoluteFilePath("../python/templates");
    if (!QFile::exists(templatesPath)) {
        templatesPath = QDir(appPath).absoluteFilePath("python/templates");
    }

    QString fullPath = QDir(templatesPath).absoluteFilePath(templateName);
    if (!QFile::exists(fullPath)) {
        std::cerr << "Error: Template not found: " << qPrintable(templateName) << std::endl;
        return 1;
    }

    // Read template
    QFile file(fullPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::cerr << "Error: Cannot open template: " << qPrintable(fullPath) << std::endl;
        return 1;
    }
    
    QTextStream in(&file);
    QString code = in.readAll();
    file.close();

    if (!quiet) {
        std::cout << "Running FluxScript Template: " << qPrintable(templateName) << std::endl;
    }

#ifdef HAVE_FLUXSCRIPT
    // Initialize JIT
    Flux::JITEngine::instance().initialize();
    
    // Wrap code if it looks like a snippet
    QString wrappedCode = code;
    if (!code.contains("def update") && !code.contains("def main")) {
        wrappedCode = "def update(t, inputs) {\n" + code + "\n}";
    }

    // Compile script
    std::string error;
    if (!Flux::JITEngine::instance().compileScript(wrappedCode.toStdString(), &error)) {
        std::cerr << "Compilation Error in Template: " << error << std::endl;
        return 1;
    }
    
    // Check if netlist generation only
    if (parser.isSet("netlist")) {
        return generateNetlist(code, fullPath, outputFile, quiet);
    }
    
    // Run simulation
    if (parser.isSet("run")) {
        return runSimulation(fullPath, outputFile, 
                           parser.value("tran"), quiet);
    }
    
    // Execute
    QString tStr = parser.value("time");
    if (tStr.isEmpty()) tStr = globalParser.value("time");
    
    QString inputsStr = parser.value("inputs");
    if (inputsStr.isEmpty()) inputsStr = globalParser.value("inputs");

    double tVal = tStr.toDouble();
    QStringList inputStrs = inputsStr.split(",", Qt::SkipEmptyParts);
    std::vector<double> inputs;
    for (const QString& s : inputStrs) {
        inputs.push_back(s.toDouble());
    }
    // Pad to at least a few inputs to avoid index out of bounds in common templates
    while (inputs.size() < 10) inputs.push_back(0.0);

    Flux::FluxValue result = 0.0;
    void* addr = Flux::JITEngine::instance().getFunctionPointer("update");
    if (addr) {
        typedef double (*UpdateFn)(double, const double*);
        result = reinterpret_cast<UpdateFn>(addr)(tVal, inputs.data());
    } else {
        addr = Flux::JITEngine::instance().getFunctionPointer("main");
        if (addr) {
            typedef double (*MainFn)();
            result = reinterpret_cast<MainFn>(addr)();
        } else {
             std::cerr << "Execution Error: update() or main() function not found" << std::endl;
             return 1;
        }
    }

    if (parser.isSet("json")) {
        QJsonObject res;
        if (std::holds_alternative<double>(result)) res["value"] = std::get<double>(result);
        std::cout << QJsonDocument(res).toJson(QJsonDocument::Compact).constData() << std::endl;
    } else {
        if (std::holds_alternative<double>(result)) {
            std::cout << "= " << std::get<double>(result) << std::endl;
        }
    }

    if (!quiet) {
        std::cout << "✓ Template execution successful" << std::endl;
    }
#else
    std::cerr << "Error: FluxScript support not enabled" << std::endl;
    return 1;
#endif

    return 0;
}

int FluxCommand::cmdRun(const QCommandLineParser& parser, 
                        const QStringList& positional, 
                        const QString& outputFile,
                        bool quiet) {
    if (positional.size() < 2) {
        std::cerr << "Error: Missing file argument" << std::endl;
        return 1;
    }
    
    const QString inputFile = positional[1];
    QString resolvedOutput = outputFile;
    
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
    
#ifdef HAVE_FLUXSCRIPT
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
        return generateNetlist(code, inputFile, resolvedOutput, quiet);
    }
    
    // Run simulation
    if (parser.isSet("run")) {
        return runSimulation(inputFile, resolvedOutput, 
                           parser.value("tran"), quiet);
    }
#else
    Q_UNUSED(code)
    std::cerr << "Error: FluxScript support not enabled" << std::endl;
    return 1;
#endif
    
    return 0;
}

int FluxCommand::cmdCompile(const QCommandLineParser& parser,
                            const QStringList& positional,
                            const QString& outputFile,
                            bool quiet) {
    if (positional.size() < 2) {
        std::cerr << "Error: Missing file argument" << std::endl;
        return 1;
    }
    
    const QString inputFile = positional[1];
    QString resolvedOutput = outputFile;
    
    if (resolvedOutput.isEmpty()) {
        resolvedOutput = inputFile + ".cir";
    }
    
    // Read file
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
    return generateNetlist(code, inputFile, resolvedOutput, quiet);
}

int FluxCommand::cmdEval(const QCommandLineParser& parser,
                         const QStringList& positional,
                         bool quiet) {
    (void)quiet;
    
    if (positional.size() < 2) {
        std::cerr << "Error: Missing expression argument" << std::endl;
        return 1;
    }
    
    const QString expression = positional[1];
    
#ifdef HAVE_FLUXSCRIPT
    // Initialize JIT
    Flux::JITEngine::instance().initialize();
    
    // Wrap expression
    QString wrappedExpr = QString("def main() {\n%1\n}").arg(expression);

    // Compile expression
    std::string error;
    if (!Flux::JITEngine::instance().compileScript(wrappedExpr.toStdString(), &error)) {
        std::cerr << "Compilation Error: " << error << std::endl;
        return 1;
    }
    
    // Execute and get result
    Flux::FluxValue result = 0.0;
    void* addr = Flux::JITEngine::instance().getFunctionPointer("main");
    if (addr) {
        typedef double (*MainFn)();
        result = reinterpret_cast<MainFn>(addr)();
    } else {
        std::cerr << "Execution Error: Expression compilation failed to produce main()" << std::endl;
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
#else
    std::cerr << "Error: FluxScript support not enabled" << std::endl;
    Q_UNUSED(expression)
    return 1;
#endif
    
    return 0;
}

int FluxCommand::cmdRepl(const QCommandLineParser& parser,
                         const QStringList& positional,
                         bool quiet) {
    (void)parser;
    (void)positional;
    (void)quiet;
    
    std::cout << "FluxScript REPL (Read-Eval-Print Loop)" << std::endl;
    std::cout << "Type 'exit' or Ctrl+D to quit" << std::endl;
    std::cout << std::endl;
    
#ifdef HAVE_FLUXSCRIPT
    Flux::JITEngine::instance().initialize();
    
    std::string stdLine;
    int counter = 0;
    
    while (true) {
        std::cout << "flux[" << counter++ << "]> " << std::flush;
        
        if (!std::getline(std::cin, stdLine)) {
            break;
        }
        
        QString line = QString::fromStdString(stdLine).trimmed();
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
#else
    std::cerr << "Error: FluxScript support not enabled" << std::endl;
#endif
    
    return 0;
}

int FluxCommand::generateNetlist(const QString& code,
                                  const QString& inputFile,
                                  const QString& outputFile,
                                  bool quiet) {
#ifdef HAVE_FLUXSCRIPT
    Flux::CompilerOptions options;
    options.inputName = inputFile.toStdString();
    Flux::CompilerInstance compiler(options);
    
    std::string error;
    auto ast = compiler.parse(code.toStdString(), &error);
    
    if (!ast || !error.empty()) {
        std::cerr << "Parsing Error: " << error << std::endl;
        return 1;
    }
    
    Flux::NetlistGenerator generator;
    std::string netlistStr = generator.generateNetlist(
        ast->functions, ast->subckts, ast->models, ast->analyses, ast->measures);
    
    QString netlist = QString::fromStdString(netlistStr);
#else
    (void)code;
    (void)inputFile;
    QString netlist = "* FluxScript support disabled\n.END\n";
#endif
    
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
    
#ifdef HAVE_FLUXSCRIPT
    // Initialize ngspice bridge
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
                double time = Flux::flux_ngspice_get_vector("time", i);
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
#else
    (void)tStep;
    (void)tStop;
    (void)outputFile;
    std::cerr << "Error: FluxScript support not enabled" << std::endl;
    return 1;
#endif
    
    return 0;
}

void FluxCommand::printHelp() {
    std::cout << "viora flux - FluxScript SPICE Integration\n";
    std::cout << "\n";
    std::cout << "Usage: viora flux <subcommand> [options] [file.flux]\n";
    std::cout << "\n";
    std::cout << "Subcommands:\n";
    std::cout << "  run <file.flux>     Compile and run FluxScript file\n";
    std::cout << "  compile <file.flux> Generate SPICE netlist\n";
    std::cout << "  eval <expression>   Evaluate FluxScript expression\n";
    std::cout << "  repl                Interactive REPL mode\n";
    std::cout << "  template-list       List available FluxScript templates\n";
    std::cout << "  template-run <name> Run a specific template\n";
    std::cout << "\n";
    std::cout << "Options:\n";
    std::cout << "  --out <file>        Output file for results\n";
    std::cout << "  --netlist           Generate netlist only\n";
    std::cout << "  --run               Run simulation\n";
    std::cout << "  --tran <tstep,tstop> Transient parameters\n";
    std::cout << "  --time <value>      Time value for template execution (default 0.0)\n";
    std::cout << "  --inputs <v1,v2...> Input values for template (default 0.0)\n";
    std::cout << "  --json              Output as JSON\n";
    std::cout << "  -q, --quiet         Quiet mode\n";
    std::cout << "  -h, --help          Show help\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  viora flux run circuit.flux\n";
    std::cout << "  viora flux template-run pwm_generator --time 0.0005 --inputs 0.8\n";
    std::cout << "  viora flux eval \"sin(pi/2)\"\n";
    std::cout << "  viora flux repl\n";
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
