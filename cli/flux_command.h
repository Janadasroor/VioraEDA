#ifndef VIOSPICE_FLUX_COMMAND_H
#define VIOSPICE_FLUX_COMMAND_H

#include <QObject>
#include <QStringList>

class QCommandLineParser;

namespace VioSpice {

/**
 * @brief FluxScript integration command for vio-cmd CLI
 * 
 * Provides subcommands for:
 * - run: Compile and run FluxScript files
 * - compile: Generate SPICE netlists
 * - eval: Evaluate expressions
 * - repl: Interactive REPL mode
 */
class FluxCommand : public QObject {
    Q_OBJECT
    
public:
    explicit FluxCommand(QObject* parent = nullptr);
    
    /**
     * @brief Run flux subcommand
     * @param args Command line arguments (after "flux")
     * @param quiet Quiet mode flag
     * @return Exit code (0 = success)
     */
    int run(const QStringList& args, const QCommandLineParser& globalParser, bool quiet = false);

private:
    // Subcommand handlers
    int cmdRun(const QCommandLineParser& parser, 
               const QStringList& positional,
               const QString& outputFile,
               bool quiet);
    int cmdCompile(const QCommandLineParser& parser,
                   const QStringList& positional,
                   const QString& outputFile,
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
