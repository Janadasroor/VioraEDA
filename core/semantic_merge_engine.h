#ifndef SEMANTIC_MERGE_ENGINE_H
#define SEMANTIC_MERGE_ENGINE_H

#include <QString>
#include <QList>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariant>

/**
 * @brief Types of changes detected during semantic merge.
 */
enum class ChangeType {
    NoChange,
    Added,
    Removed,
    Modified,
    Conflict       // Both sides modified the same field differently
};

/**
 * @brief A single semantic change detected between versions.
 */
struct SemanticChange {
    ChangeType type;
    QString path;           // JSON path, e.g. "components[2].footprint"
    QString objectId;       // Unique ID of the object (if applicable)
    QVariant baseValue;     // Value in base version
    QVariant oursValue;     // Value in "ours" version
    QVariant theirsValue;   // Value in "theirs" version
    QVariant mergedValue;   // Resolved value (after auto-merge or conflict resolution)
    bool resolved = false;  // Whether conflict has been resolved
};

/**
 * @brief Result of a semantic merge operation.
 */
struct MergeResult {
    bool success = false;
    bool hasConflicts = false;
    QJsonObject mergedDocument;
    QList<SemanticChange> changes;
    QString errorMessage;
    
    int addedCount = 0;
    int removedCount = 0;
    int modifiedCount = 0;
    int conflictCount = 0;
    int autoMergedCount = 0;
};

/**
 * @brief Semantic 3-way merge engine for EDA JSON files.
 * 
 * Unlike line-based text merge (which produces unreadable conflicts for JSON),
 * this engine understands JSON structure and can:
 * - Match objects by unique "id" field
 * - Auto-merge non-conflicting changes (different fields, different objects)
 * - Detect conflicts when the same field of the same object is modified differently
 * - Produce a clean merged JSON document
 * 
 * Usage:
 *   MergeResult result = SemanticMergeEngine::merge(baseJson, oursJson, theirsJson);
 *   if (result.hasConflicts) {
 *       // Show conflict resolution UI
 *   }
 *   // result.mergedDocument contains the merged result
 */
class SemanticMergeEngine {
public:
    /**
     * @brief Perform a semantic 3-way merge on two JSON documents.
     * @param base The common ancestor (original version)
     * @param ours Our changes (current working copy)
     * @param theirs Their changes (incoming version to merge)
     * @return MergeResult with merged document and any conflicts
     */
    static MergeResult merge(const QJsonObject& base, 
                              const QJsonObject& ours, 
                              const QJsonObject& theirs);

    /**
     * @brief Merge two JSON files from disk.
     * @param basePath Path to base/ancestor file
     * @param oursPath Path to "ours" file
     * @param theirsPath Path to "theirs" file
     * @param outputPath Path to write merged result
     * @return true on success, check errorMessage on failure
     */
    static bool mergeFiles(const QString& basePath,
                           const QString& oursPath,
                           const QString& theirsPath,
                           const QString& outputPath,
                           QString* errorMessage = nullptr);

    /**
     * @brief Check if a JSON value is an array of objects (for special array handling).
     */
    static bool isObjectArray(const QJsonValue& value);

    /**
     * @brief Get a unique ID from a JSON object, or generate one from content.
     */
    static QString getObjectId(const QJsonObject& obj);

    /**
     * @brief Resolve a single conflict by choosing a value.
     * @param change The conflict change to resolve
     * @param useOurs true to use "ours" value, false to use "theirs" value
     */
    static void resolveConflict(SemanticChange& change, bool useOurs);

private:
    // Internal merge helpers
    static void mergeObjects(const QJsonObject& base,
                             const QJsonObject& ours,
                             const QJsonObject& theirs,
                             QJsonObject& result,
                             QList<SemanticChange>& changes,
                             const QString& pathPrefix);

    static void mergeArrays(const QJsonArray& base,
                            const QJsonArray& ours,
                            const QJsonArray& theirs,
                            QJsonArray& result,
                            QList<SemanticChange>& changes,
                            const QString& pathPrefix);

    static void trackChange(ChangeType type,
                           const QString& path,
                           const QString& objectId,
                           const QVariant& baseVal,
                           const QVariant& oursVal,
                           const QVariant& theirsVal,
                           const QVariant& mergedVal,
                           QList<SemanticChange>& changes);
};

#endif // SEMANTIC_MERGE_ENGINE_H
