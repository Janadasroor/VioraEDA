// design_rule_engine.cpp
// Multi-threaded design rule evaluation engine

#include "design_rule_engine.h"
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QtConcurrent/QtConcurrent>
#include <QThreadPool>
#include <QThread>
#include <QElapsedTimer>
#include <QCryptographicHash>
#include "../schematic/items/schematic_item.h"
#include "../schematic/analysis/net_manager.h"

namespace Flux {
namespace Core {

// ────────────────────────────────────────────────────────────────────────────
// DesignRuleEngine Implementation
// ────────────────────────────────────────────────────────────────────────────

DesignRuleEngine::DesignRuleEngine(QObject* parent)
    : QObject(parent)
    , m_ruleSet(nullptr)
    , m_running(false)
    , m_cancelled(false)
    , m_watcher(nullptr)
    , m_cacheEnabled(true)
    , m_threadPool(QThreadPool::globalInstance())
{
    m_config = defaultConfig();
    
    m_watcher = new QFutureWatcher<void>(this);
    connect(m_watcher, &QFutureWatcher<void>::finished,
            this, &DesignRuleEngine::onFutureFinished);
    connect(m_watcher, &QFutureWatcher<void>::progressValueChanged,
            this, &DesignRuleEngine::onProgressUpdated);
}

DesignRuleEngine::~DesignRuleEngine() {
    cancel();
    m_watcher->deleteLater();
}

void DesignRuleEngine::setConfig(const RuleEngineConfig& config) {
    m_config = config;
}

void DesignRuleEngine::setRuleSet(DesignRuleSet* ruleSet) {
    m_ruleSet = ruleSet;
}

void DesignRuleEngine::setErcRules(const SchematicERCRules& rules) {
    m_ercRules = rules;
}

void DesignRuleEngine::setCacheEnabled(bool enabled) {
    m_cacheEnabled = enabled;
}

void DesignRuleEngine::clearCache() {
    QMutexLocker locker(&m_cacheMutex);
    m_resultCache.clear();
}

int DesignRuleEngine::optimalThreadCount() {
    int threads = QThread::idealThreadCount();
    // Leave one thread for UI
    return qMax(1, threads - 1);
}

RuleEngineConfig DesignRuleEngine::defaultConfig() {
    RuleEngineConfig config;
    config.parallelExecution = true;
    config.threadCount = optimalThreadCount();
    config.incrementalCheck = false;
    config.timeoutMs = 5000;
    config.minSeverity = RuleSeverity::Info;
    return config;
}

void DesignRuleEngine::run(QGraphicsScene* scene, NetManager* netManager) {
    if (!scene || m_running) return;

    m_running = true;
    m_cancelled = false;
    m_violations.clear();
    m_stats = RuleExecutionStats();

    QElapsedTimer timer;
    timer.start();

    executeRules(scene, netManager);

    m_stats.totalTimeMs = timer.elapsed();
    m_running = false;

    // Sort violations by severity
    sortViolations(m_violations);

    emit executionFinished(m_violations);
}

void DesignRuleEngine::runAsync(QGraphicsScene* scene, NetManager* netManager) {
    if (!scene || m_running) return;

    m_running = true;
    m_cancelled = false;
    m_violations.clear();
    m_stats = RuleExecutionStats();

    emit executionStarted();

    m_future = QtConcurrent::run([this, scene, netManager]() {
        executeRules(scene, netManager);
    });

    m_watcher->setFuture(m_future);
}

void DesignRuleEngine::cancel() {
    m_cancelled = true;
    if (m_watcher) {
        m_watcher->cancel();
    }
    m_running = false;
    emit executionCancelled();
}

void DesignRuleEngine::executeRules(QGraphicsScene* scene, NetManager* netManager) {
    if (!m_ruleSet || m_cancelled) return;

    QElapsedTimer execTimer;
    execTimer.start();

    int totalRules = m_ruleSet->ruleCount();
    int rulesCompleted = 0;

    // Collect enabled rules
    QList<DesignRule*> enabledRules;
    for (auto* rule : m_ruleSet->rules()) {
        if (rule->enabled() && !m_cancelled) {
            if (m_config.enabledCategories.isEmpty() ||
                m_config.enabledCategories.contains(static_cast<int>(rule->category()))) {
                enabledRules.append(rule);
            }
        }
    }

    totalRules = enabledRules.count();

    if (m_config.parallelExecution && m_config.threadCount > 1) {
        // Parallel execution
        QList<QFuture<QList<DesignRuleViolation>>> futures;
        
        for (auto* rule : enabledRules) {
            if (m_cancelled) break;

            RuleProgress progress;
            progress.currentRule = rulesCompleted++;
            progress.totalRules = totalRules;
            progress.currentRuleName = rule->name();
            progress.percentComplete = (rulesCompleted * 100) / totalRules;
            emit progressUpdated(progress);

            auto future = QtConcurrent::run(m_threadPool, [this, rule, scene, netManager]() {
                if (m_cancelled) return QList<DesignRuleViolation>();
                return evaluateRule(rule, scene, netManager);
            });
            futures.append(future);
        }

        // Collect results
        for (auto& future : futures) {
            if (m_cancelled) break;
            
            auto violations = future.result();
            if (!violations.isEmpty()) {
                QMutexLocker locker(&m_violationsMutex);
                m_violations.append(violations);
                m_stats.violationsFound += violations.count();
                emit batchViolationsFound(violations);
            }
            m_stats.rulesEvaluated++;
        }

        m_stats.threadsUsed = m_config.threadCount;

    } else {
        // Sequential execution
        for (auto* rule : enabledRules) {
            if (m_cancelled) break;

            RuleProgress progress;
            progress.currentRule = rulesCompleted++;
            progress.totalRules = totalRules;
            progress.currentRuleName = rule->name();
            progress.percentComplete = (rulesCompleted * 100) / totalRules;
            emit progressUpdated(progress);

            auto violations = evaluateRule(rule, scene, netManager);
            if (!violations.isEmpty()) {
                QMutexLocker locker(&m_violationsMutex);
                m_violations.append(violations);
                m_stats.violationsFound += violations.count();
                emit batchViolationsFound(violations);
            }
            m_stats.rulesEvaluated++;

            emit ruleEvaluated(rule->name(), violations.count());
        }

        m_stats.threadsUsed = 1;
    }

    // Run ERC checks
    if (!m_cancelled) {
        auto ercViolations = checkErcRules(scene, netManager);
        if (!ercViolations.isEmpty()) {
            QMutexLocker locker(&m_violationsMutex);
            m_violations.append(ercViolations);
            m_stats.violationsFound += ercViolations.count();
        }
    }

    // Run DRC checks
    if (!m_cancelled) {
        auto drcViolations = checkDrcRules(scene, netManager);
        if (!drcViolations.isEmpty()) {
            QMutexLocker locker(&m_violationsMutex);
            m_violations.append(drcViolations);
            m_stats.violationsFound += drcViolations.count();
        }
    }

    // Filter by severity
    filterViolationsBySeverity(m_violations);

    // Count items checked
    m_stats.itemsChecked = scene->items().count();
    m_stats.totalTimeMs = execTimer.elapsed();
}

QList<DesignRuleViolation> DesignRuleEngine::evaluateRule(
    DesignRule* rule, 
    QGraphicsScene* scene, 
    NetManager* netManager
) {
    QList<DesignRuleViolation> violations;

    if (!rule || !rule->enabled()) {
        return violations;
    }

    // Check cache
    if (m_cacheEnabled) {
        QString cacheKey = getCacheKey(rule, scene);
        QList<DesignRuleViolation> cached;
        if (getCachedResult(cacheKey, cached)) {
            return cached;
        }
    }

    // Evaluate based on category
    if (rule->category() == RuleCategory::Custom) {
        violations = evaluateCustomRule(rule, scene, netManager);
    }
    // Other categories handled by specific check functions

    // Cache result
    if (m_cacheEnabled && !violations.isEmpty()) {
        QString cacheKey = getCacheKey(rule, scene);
        cacheResult(cacheKey, violations);
    }

    return violations;
}

QList<DesignRuleViolation> DesignRuleEngine::checkErcRules(
    QGraphicsScene* scene, 
    NetManager* netManager
) {
    QList<DesignRuleViolation> violations;

    // TODO: Integrate with existing SchematicERC system
    // For now, return empty list
    // Future: Convert SchematicERC violations to DesignRuleViolation format

    return violations;
}

QList<DesignRuleViolation> DesignRuleEngine::checkDrcRules(
    QGraphicsScene* scene,
    NetManager* netManager
) {
    QList<DesignRuleViolation> violations;

    // Get all schematic items
    QList<SchematicItem*> schematicItems;
    for (auto* item : scene->items()) {
        if (auto* si = dynamic_cast<SchematicItem*>(item)) {
            schematicItems.append(si);
        }
    }

    // Check for missing values
    for (auto* rule : m_ruleSet->rulesByCategory(RuleCategory::DRC)) {
        if (!rule->enabled() || m_cancelled) continue;

        if (rule->name() == "Missing Component Values") {
            for (auto* item : schematicItems) {
                if (item->value().isEmpty() && !item->reference().isEmpty()) {
                    DesignRuleViolation v;
                    v.ruleId = rule->id();
                    v.ruleName = rule->name();
                    v.severity = rule->defaultSeverity();
                    v.message = QString("Component %1 has no value").arg(item->reference());
                    v.objectRef = item->reference();
                    v.position = item->scenePos();
                    v.context["itemType"] = item->itemTypeName();
                    violations.append(v);
                }
            }
        }

        if (rule->name() == "Missing Footprint") {
            for (auto* item : schematicItems) {
                QString footprint = item->property("footprint").toString();
                if (footprint.isEmpty() && !item->reference().isEmpty()) {
                    DesignRuleViolation v;
                    v.ruleId = rule->id();
                    v.ruleName = rule->name();
                    v.severity = rule->defaultSeverity();
                    v.message = QString("Component %1 has no footprint").arg(item->reference());
                    v.objectRef = item->reference();
                    v.position = item->scenePos();
                    violations.append(v);
                }
            }
        }
    }

    return violations;
}

QList<DesignRuleViolation> DesignRuleEngine::evaluateCustomRule(
    DesignRule* rule,
    QGraphicsScene* scene,
    NetManager* netManager
) {
    QList<DesignRuleViolation> violations;

    if (rule->expression().isEmpty()) {
        return violations;
    }

    // TODO: Implement Python/QJS expression evaluation
    // For now, return empty list
    // Future: Integrate with Python rule engine

    return violations;
}

QString DesignRuleEngine::getCacheKey(DesignRule* rule, QGraphicsScene* scene) {
    // Create unique key based on rule ID and scene content hash
    QByteArray data;
    data.append(rule->id().toByteArray());
    data.append(QByteArray::number(scene->itemsBoundingRect().width()));
    data.append(QByteArray::number(scene->itemsBoundingRect().height()));
    data.append(QByteArray::number(scene->items().count()));

    QString hash = QString::fromLatin1(
        QCryptographicHash::hash(data, QCryptographicHash::Md5).toHex()
    );

    return QString("rule_%1_scene_%2").arg(rule->id().toString(), hash);
}

bool DesignRuleEngine::getCachedResult(const QString& key, QList<DesignRuleViolation>& result) {
    QMutexLocker locker(&m_cacheMutex);
    if (m_resultCache.contains(key)) {
        result = m_resultCache[key];
        return true;
    }
    return false;
}

void DesignRuleEngine::cacheResult(const QString& key, const QList<DesignRuleViolation>& result) {
    QMutexLocker locker(&m_cacheMutex);
    m_resultCache[key] = result;
}

void DesignRuleEngine::filterViolationsBySeverity(QList<DesignRuleViolation>& violations) {
    if (m_config.minSeverity == RuleSeverity::Info) {
        return; // Keep all
    }

    QList<DesignRuleViolation> filtered;
    for (const auto& v : violations) {
        if (static_cast<int>(v.severity) >= static_cast<int>(m_config.minSeverity)) {
            filtered.append(v);
        }
    }
    violations = filtered;
}

void DesignRuleEngine::sortViolations(QList<DesignRuleViolation>& violations) {
    std::sort(violations.begin(), violations.end());
}

void DesignRuleEngine::onFutureFinished() {
    m_running = false;
    m_stats.totalTimeMs = m_watcher->property("elapsedTime").toLongLong();
    emit executionFinished(m_violations);
}

void DesignRuleEngine::onProgressUpdated(int value) {
    Q_UNUSED(value);
    // Progress is handled in executeRules
}

} // namespace Core
} // namespace Flux
