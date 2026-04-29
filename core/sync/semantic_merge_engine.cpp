#include "semantic_merge_engine.h"
#include <QFile>
#include <QJsonDocument>
#include <QFileInfo>
#include <QDebug>
#include <QSet>

// ============================================================================
// Public API
// ============================================================================

MergeResult SemanticMergeEngine::merge(const QJsonObject& base,
                                        const QJsonObject& ours,
                                        const QJsonObject& theirs) {
    MergeResult result;
    result.success = true;
    
    QJsonObject merged;
    mergeObjects(base, ours, theirs, merged, result.changes, "");
    result.mergedDocument = merged;
    
    // Analyze changes
    for (const auto& change : result.changes) {
        switch (change.type) {
            case ChangeType::Added: result.addedCount++; break;
            case ChangeType::Removed: result.removedCount++; break;
            case ChangeType::Modified: 
                if (!change.resolved) result.modifiedCount++;
                break;
            case ChangeType::Conflict: result.conflictCount++; break;
            default: break;
        }
    }
    
    result.hasConflicts = result.conflictCount > 0;
    
    // Auto-apply resolved conflicts to merged document
    if (result.hasConflicts) {
        for (auto& change : result.changes) {
            if (change.type == ChangeType::Conflict && change.resolved) {
                // The merged document already has the resolved value from mergeObjects
            }
        }
    }
    
    result.autoMergedCount = result.addedCount + result.removedCount + result.modifiedCount;
    
    return result;
}

bool SemanticMergeEngine::mergeFiles(const QString& basePath,
                                      const QString& oursPath,
                                      const QString& theirsPath,
                                      const QString& outputPath,
                                      QString* errorMessage) {
    auto loadJson = [](const QString& path) -> QJsonObject {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return QJsonObject();
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) return QJsonObject();
        return doc.object();
    };
    
    QJsonObject base = loadJson(basePath);
    QJsonObject ours = loadJson(oursPath);
    QJsonObject theirs = loadJson(theirsPath);
    
    if (base.isEmpty() && ours.isEmpty() && theirs.isEmpty()) {
        if (errorMessage) *errorMessage = "Failed to load JSON files";
        return false;
    }
    
    MergeResult result = merge(base, ours, theirs);
    
    if (!result.success) {
        if (errorMessage) *errorMessage = result.errorMessage;
        return false;
    }
    
    QFile outFile(outputPath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        if (errorMessage) *errorMessage = "Cannot write output file: " + outputPath;
        return false;
    }
    
    QJsonDocument doc(result.mergedDocument);
    outFile.write(doc.toJson(QJsonDocument::Indented));
    
    return true;
}

QString SemanticMergeEngine::getObjectId(const QJsonObject& obj) {
    if (obj.contains("id") && !obj["id"].toString().isEmpty()) {
        return obj["id"].toString();
    }
    // Fallback: use name/reference if available
    if (obj.contains("name") && !obj["name"].toString().isEmpty()) {
        return obj["name"].toString();
    }
    if (obj.contains("reference") && !obj["reference"].toString().isEmpty()) {
        return obj["reference"].toString();
    }
    // Generate hash from all key-value pairs
    QString content;
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        content += it.key() + "=" + it.value().toString() + ";";
    }
    return QString::number(qHash(content));
}

bool SemanticMergeEngine::isObjectArray(const QJsonValue& value) {
    if (!value.isArray()) return false;
    QJsonArray arr = value.toArray();
    if (arr.isEmpty()) return false;
    return arr[0].isObject();
}

void SemanticMergeEngine::resolveConflict(SemanticChange& change, bool useOurs) {
    change.mergedValue = useOurs ? change.oursValue : change.theirsValue;
    change.resolved = true;
    change.type = ChangeType::Modified; // Treat as resolved modification
}

// ============================================================================
// Internal Helpers
// ============================================================================

void SemanticMergeEngine::trackChange(ChangeType type,
                                       const QString& path,
                                       const QString& objectId,
                                       const QVariant& baseVal,
                                       const QVariant& oursVal,
                                       const QVariant& theirsVal,
                                       const QVariant& mergedVal,
                                       QList<SemanticChange>& changes) {
    SemanticChange change;
    change.type = type;
    change.path = path;
    change.objectId = objectId;
    change.baseValue = baseVal;
    change.oursValue = oursVal;
    change.theirsValue = theirsVal;
    
    switch (type) {
        case ChangeType::NoChange:
            change.mergedValue = oursVal;
            break;
        case ChangeType::Added:
            change.mergedValue = theirsVal; // New in theirs
            if (oursVal.isValid()) change.mergedValue = oursVal; // Or new in ours
            break;
        case ChangeType::Removed:
            // Don't include in merged
            break;
        case ChangeType::Modified:
            if (oursVal != theirsVal) {
                // Both changed differently - CONFLICT
                change.type = ChangeType::Conflict;
                change.mergedValue = oursVal; // Default to ours
            } else {
                change.mergedValue = oursVal; // Same change, no conflict
            }
            break;
        case ChangeType::Conflict:
            change.mergedValue = oursVal; // Default to ours until resolved
            break;
    }
    
    changes.append(change);
}

void SemanticMergeEngine::mergeObjects(const QJsonObject& base,
                                        const QJsonObject& ours,
                                        const QJsonObject& theirs,
                                        QJsonObject& result,
                                        QList<SemanticChange>& changes,
                                        const QString& pathPrefix) {
    QSet<QString> allKeys;
    for (auto it = base.begin(); it != base.end(); ++it) allKeys.insert(it.key());
    for (auto it = ours.begin(); it != ours.end(); ++it) allKeys.insert(it.key());
    for (auto it = theirs.begin(); it != theirs.end(); ++it) allKeys.insert(it.key());
    
    QString objId;
    if (!ours.isEmpty()) objId = getObjectId(ours);
    else if (!theirs.isEmpty()) objId = getObjectId(theirs);
    
    for (const QString& key : allKeys) {
        QString fullPath = pathPrefix.isEmpty() ? key : pathPrefix + "." + key;
        bool inBase = base.contains(key);
        bool inOurs = ours.contains(key);
        bool inTheirs = theirs.contains(key);
        
        QVariant baseVal = inBase ? base[key].toVariant() : QVariant();
        QVariant oursVal = inOurs ? ours[key].toVariant() : QVariant();
        QVariant theirsVal = inTheirs ? theirs[key].toVariant() : QVariant();
        
        // Case 1: Key exists in all three
        if (inBase && inOurs && inTheirs) {
            QJsonValue baseJson = base[key];
            QJsonValue oursJson = ours[key];
            QJsonValue theirsJson = theirs[key];
            
            if (baseJson == oursJson && baseJson == theirsJson) {
                // No changes
                result[key] = baseJson;
                continue;
            }
            
            if (baseJson == oursJson && baseJson != theirsJson) {
                // Only theirs changed
                if (isObjectArray(theirsJson) && isObjectArray(baseJson)) {
                    QJsonArray arr;
                    mergeArrays(baseJson.toArray(), oursJson.toArray(), theirsJson.toArray(), arr, changes, fullPath);
                    result[key] = arr;
                } else if (theirsJson.isObject() && baseJson.isObject()) {
                    QJsonObject obj;
                    mergeObjects(baseJson.toObject(), oursJson.toObject(), theirsJson.toObject(), obj, changes, fullPath);
                    result[key] = obj;
                } else {
                    result[key] = theirsJson;
                    trackChange(ChangeType::Modified, fullPath, objId, baseVal, oursVal, theirsVal, oursVal, changes);
                }
            } else if (baseJson == theirsJson && baseJson != oursJson) {
                // Only ours changed
                if (isObjectArray(oursJson) && isObjectArray(baseJson)) {
                    QJsonArray arr;
                    mergeArrays(baseJson.toArray(), oursJson.toArray(), theirsJson.toArray(), arr, changes, fullPath);
                    result[key] = arr;
                } else if (oursJson.isObject() && baseJson.isObject()) {
                    QJsonObject obj;
                    mergeObjects(baseJson.toObject(), oursJson.toObject(), theirsJson.toObject(), obj, changes, fullPath);
                    result[key] = obj;
                } else {
                    result[key] = oursJson;
                    trackChange(ChangeType::Modified, fullPath, objId, baseVal, oursVal, theirsVal, oursVal, changes);
                }
            } else if (baseJson != oursJson && baseJson != theirsJson) {
                // Both changed
                if (oursJson == theirsJson) {
                    // Same change
                    result[key] = oursJson;
                    trackChange(ChangeType::Modified, fullPath, objId, baseVal, oursVal, theirsVal, oursVal, changes);
                } else if (isObjectArray(oursJson) && isObjectArray(theirsJson) && isObjectArray(baseJson)) {
                    QJsonArray arr;
                    mergeArrays(baseJson.toArray(), oursJson.toArray(), theirsJson.toArray(), arr, changes, fullPath);
                    result[key] = arr;
                } else if (oursJson.isObject() && theirsJson.isObject() && baseJson.isObject()) {
                    QJsonObject obj;
                    mergeObjects(baseJson.toObject(), oursJson.toObject(), theirsJson.toObject(), obj, changes, fullPath);
                    result[key] = obj;
                } else {
                    // CONFLICT - different scalar values
                    result[key] = oursJson; // Default to ours
                    QVariant merged;
                    trackChange(ChangeType::Conflict, fullPath, objId, baseVal, oursVal, theirsVal, merged, changes);
                }
            }
        }
        // Case 2: Key only in ours (added by us)
        else if (!inBase && inOurs && !inTheirs) {
            result[key] = ours[key];
            trackChange(ChangeType::Added, fullPath, objId, baseVal, oursVal, theirsVal, oursVal, changes);
        }
        // Case 3: Key only in theirs (added by them)
        else if (!inBase && !inOurs && inTheirs) {
            result[key] = theirs[key];
            trackChange(ChangeType::Added, fullPath, objId, baseVal, oursVal, theirsVal, theirsVal, changes);
        }
        // Case 4: Key in base and ours, but not theirs (removed by them)
        else if (inBase && inOurs && !inTheirs) {
            // They deleted it - this is a conflict if we modified it
            if (base[key] != ours[key]) {
                result[key] = ours[key]; // Keep ours (modified)
                trackChange(ChangeType::Conflict, fullPath, objId, baseVal, oursVal, theirsVal, oursVal, changes);
            } else {
                // We didn't modify, they deleted - accept deletion
                // Don't add to result
                trackChange(ChangeType::Removed, fullPath, objId, baseVal, oursVal, theirsVal, QVariant(), changes);
            }
        }
        // Case 5: Key in base and theirs, but not ours (removed by us)
        else if (inBase && !inOurs && inTheirs) {
            // We deleted it - this is a conflict if they modified it
            if (base[key] != theirs[key]) {
                result[key] = theirs[key]; // Keep theirs (modified)
                trackChange(ChangeType::Conflict, fullPath, objId, baseVal, oursVal, theirsVal, theirsVal, changes);
            } else {
                // They didn't modify, we deleted - accept deletion
                trackChange(ChangeType::Removed, fullPath, objId, baseVal, oursVal, theirsVal, QVariant(), changes);
            }
        }
        // Case 6: Key in all three but ours removed it
        // (handled above)
    }
}

void SemanticMergeEngine::mergeArrays(const QJsonArray& base,
                                       const QJsonArray& ours,
                                       const QJsonArray& theirs,
                                       QJsonArray& result,
                                       QList<SemanticChange>& changes,
                                       const QString& pathPrefix) {
    // Build maps of objects by ID
    auto buildMap = [](const QJsonArray& arr) -> QMap<QString, QJsonObject> {
        QMap<QString, QJsonObject> map;
        for (int i = 0; i < arr.size(); ++i) {
            if (arr[i].isObject()) {
                QJsonObject obj = arr[i].toObject();
                QString id = getObjectId(obj);
                if (!id.isEmpty()) {
                    // Store with index as suffix for duplicate IDs
                    if (map.contains(id)) {
                        id = id + "_" + QString::number(i);
                    }
                    map[id] = obj;
                }
            }
        }
        return map;
    };
    
    // Preserve order from base, then add new items
    QMap<QString, QJsonObject> baseMap = buildMap(base);
    QMap<QString, QJsonObject> oursMap = buildMap(ours);
    QMap<QString, QJsonObject> theirsMap = buildMap(theirs);
    
    QSet<QString> allIds;
    for (auto it = baseMap.begin(); it != baseMap.end(); ++it) allIds.insert(it.key());
    for (auto it = oursMap.begin(); it != oursMap.end(); ++it) allIds.insert(it.key());
    for (auto it = theirsMap.begin(); it != theirsMap.end(); ++it) allIds.insert(it.key());
    
    // Track order from base
    QStringList baseOrder;
    for (int i = 0; i < base.size(); ++i) {
        if (base[i].isObject()) {
            baseOrder.append(getObjectId(base[i].toObject()));
        }
    }
    
    // Merge objects that exist in base
    for (const QString& id : baseOrder) {
        if (!allIds.contains(id)) continue;
        bool inBase = baseMap.contains(id);
        bool inOurs = oursMap.contains(id);
        bool inTheirs = theirsMap.contains(id);
        
        if (inBase && inOurs && inTheirs) {
            QJsonObject merged;
            mergeObjects(baseMap[id], oursMap[id], theirsMap[id], merged, changes, pathPrefix + "[" + id + "]");
            result.append(merged);
        } else if (inBase && inOurs && !inTheirs) {
            // They removed it
            if (baseMap[id] != oursMap[id]) {
                // We modified it, they removed - conflict, keep ours
                result.append(oursMap[id]);
            }
            // else: they removed, we didn't modify - skip (deleted)
        } else if (inBase && !inOurs && inTheirs) {
            // We removed it
            if (baseMap[id] != theirsMap[id]) {
                // They modified it, we removed - conflict, keep theirs
                result.append(theirsMap[id]);
            }
            // else: we removed, they didn't modify - skip (deleted)
        } else if (!inBase && inOurs && inTheirs) {
            // Added by both (shouldn't happen often for arrays)
            if (oursMap[id] == theirsMap[id]) {
                result.append(oursMap[id]);
            } else {
                result.append(oursMap[id]); // Default to ours
            }
        } else if (!inBase && inOurs && !inTheirs) {
            result.append(oursMap[id]);
        } else if (!inBase && !inOurs && inTheirs) {
            result.append(theirsMap[id]);
        }
    }
    
    // Add new items from ours (not in base)
    for (auto it = oursMap.begin(); it != oursMap.end(); ++it) {
        if (!baseMap.contains(it.key()) && !theirsMap.contains(it.key())) {
            result.append(it.value());
        }
    }
    
    // Add new items from theirs (not in base)
    for (auto it = theirsMap.begin(); it != theirsMap.end(); ++it) {
        if (!baseMap.contains(it.key()) && !oursMap.contains(it.key())) {
            result.append(it.value());
        }
    }
}
