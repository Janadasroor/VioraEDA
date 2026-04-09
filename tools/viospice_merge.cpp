#include "../core/semantic_merge_engine.h"
#include "../core/merge_conflict_dialog.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QTextStream>
#include <QFileInfo>

/**
 * @brief CLI tool for semantic merge of EDA JSON files.
 * 
 * Usage:
 *   viospice-merge <base> <ours> <theirs> <output> [--auto]
 * 
 * Arguments:
 *   base     - Path to common ancestor file
 *   ours     - Path to current working copy
 *   theirs   - Path to incoming version
 *   output   - Path to write merged result
 *   --auto   - Auto-resolve conflicts (use ours for conflicts)
 * 
 * Exit codes:
 *   0 - Merge successful (with or without auto-resolved conflicts)
 *   1 - Unresolved conflicts remain
 *   2 - Error (invalid JSON, file not found, etc.)
 */

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    
    if (argc < 5) {
        qWarning() << "Usage: viospice-merge <base> <ours> <theirs> <output> [--auto]";
        return 2;
    }
    
    QString basePath = QString::fromLocal8Bit(argv[1]);
    QString oursPath = QString::fromLocal8Bit(argv[2]);
    QString theirsPath = QString::fromLocal8Bit(argv[3]);
    QString outputPath = QString::fromLocal8Bit(argv[4]);
    bool autoResolve = (argc > 5 && QString::fromLocal8Bit(argv[5]) == "--auto");
    
    // Load JSON files
    auto loadJson = [](const QString& path, QString* error) -> QJsonObject {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            if (error) *error = "Cannot open: " + path;
            return QJsonObject();
        }
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
        if (err.error != QJsonParseError::NoError) {
            if (error) *error = "JSON parse error in " + path + ": " + err.errorString();
            return QJsonObject();
        }
        if (!doc.isObject()) {
            if (error) *error = path + " is not a JSON object";
            return QJsonObject();
        }
        return doc.object();
    };
    
    QString error;
    QJsonObject base = loadJson(basePath, &error);
    if (!error.isEmpty()) {
        // Base might not exist (new file) - use empty object
        if (!QFileInfo::exists(basePath)) {
            base = QJsonObject();
        } else {
            qWarning() << error;
            return 2;
        }
    }
    
    QJsonObject ours = loadJson(oursPath, &error);
    if (!error.isEmpty()) {
        qWarning() << error;
        return 2;
    }
    
    QJsonObject theirs = loadJson(theirsPath, &error);
    if (!error.isEmpty()) {
        qWarning() << error;
        return 2;
    }
    
    // Perform merge
    MergeResult result = SemanticMergeEngine::merge(base, ours, theirs);
    
    if (!result.success) {
        qWarning() << "Merge failed:" << result.errorMessage;
        return 2;
    }
    
    // Handle conflicts
    if (result.hasConflicts) {
        if (autoResolve) {
            // Auto-resolve all conflicts using ours
            for (auto& change : result.changes) {
                if (change.type == ChangeType::Conflict && !change.resolved) {
                    SemanticMergeEngine::resolveConflict(change, true);
                }
            }
            result.hasConflicts = false;
            qInfo() << "Auto-resolved" << result.conflictCount << "conflicts (using ours)";
        } else {
            // Write current merged state with conflict markers
            // Git will handle the conflict markers
            QFile outFile(outputPath);
            if (outFile.open(QIODevice::WriteOnly)) {
                QJsonDocument doc(result.mergedDocument);
                outFile.write(doc.toJson(QJsonDocument::Indented));
            }
            qWarning() << "Merge conflicts detected:" << result.conflictCount;
            qWarning() << "Resolved conflicts:" << (result.changes.size() - result.conflictCount);
            // Return 1 to signal conflicts to Git
            return 1;
        }
    }
    
    // Write merged result
    QFile outFile(outputPath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        qWarning() << "Cannot write output:" << outputPath;
        return 2;
    }
    
    QJsonDocument doc(result.mergedDocument);
    outFile.write(doc.toJson(QJsonDocument::Indented));
    
    // Summary
    qInfo() << "Merge complete:";
    qInfo() << "  Added:" << result.addedCount;
    qInfo() << "  Removed:" << result.removedCount;
    qInfo() << "  Modified:" << result.modifiedCount;
    qInfo() << "  Conflicts:" << result.conflictCount;
    qInfo() << "  Auto-merged:" << result.autoMergedCount;
    
    return 0;
}
