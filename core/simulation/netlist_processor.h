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
     * Reads a netlist file, corrects WAV paths, strips control blocks,
     * and ensures proper headers/end markers are present.
     */
    static Result process(const QString& netlistPath);

private:
    static void resolveWavPaths(QStringList& lines, const QDir& baseDir);
    static QString resolveCaseInsensitiveFilePath(const QString& path);
    static void ensureHeaderAndEnd(QStringList& lines);
    static void stripControlBlocks(QStringList& lines);
};

} // namespace Flux

#endif // NETLIST_PROCESSOR_H
