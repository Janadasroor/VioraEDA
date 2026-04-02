#ifndef DESIGN_RULE_ENGINE_H
#define DESIGN_RULE_ENGINE_H

#include <QObject>
#include <QThread>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <QMutex>
#include <QSemaphore>
#include <QElapsedTimer>
#include <QThreadPool>
#include "../core/design_rule.h"
#include "../schematic/analysis/schematic_erc_rules.h"

class QGraphicsScene;
class NetManager;

namespace Flux {
namespace Core {

/**
 * @brief Configuration for rule engine execution
 */
struct RuleEngineConfig {
    bool parallelExecution = true;      ///< Enable multi-threaded execution
    int threadCount = -1;               ///< Number of threads (-1 = auto)
    bool incrementalCheck = false;      ///< Only check modified items
    int timeoutMs = 5000;               ///< Maximum execution time
    RuleSeverity minSeverity = RuleSeverity::Info; ///< Minimum severity to report
    QList<int> enabledCategories;       ///< Categories to check (empty = all)
    QString scopeFilePath;              ///< Limit to specific file (empty = all)
};

/**
 * @brief Statistics about rule execution
 */
struct RuleExecutionStats {
    qint64 totalTimeMs;
    int rulesEvaluated;
    int violationsFound;
    int itemsChecked;
    int threadsUsed;
    bool timedOut;
    QString errorMessage;

    RuleExecutionStats() 
        : totalTimeMs(0), rulesEvaluated(0), violationsFound(0), 
          itemsChecked(0), threadsUsed(1), timedOut(false) {}
};

/**
 * @brief Progress information during rule evaluation
 */
struct RuleProgress {
    int currentRule;
    int totalRules;
    QString currentRuleName;
    int percentComplete;

    RuleProgress() : currentRule(0), totalRules(0), percentComplete(0) {}
};

/**
 * @brief Multi-threaded design rule evaluation engine
 * 
 * Features:
 * - Parallel rule evaluation across multiple threads
 * - Incremental checking (only modified items)
 * - Progress reporting
 * - Timeout handling
 * - Caching for performance
 */
class DesignRuleEngine : public QObject {
    Q_OBJECT

public:
    explicit DesignRuleEngine(QObject* parent = nullptr);
    ~DesignRuleEngine();

    // Configuration
    void setConfig(const RuleEngineConfig& config);
    RuleEngineConfig config() const { return m_config; }

    // Rule set management
    void setRuleSet(DesignRuleSet* ruleSet);
    DesignRuleSet* ruleSet() const { return m_ruleSet; }

    // Execution
    void run(QGraphicsScene* scene, NetManager* netManager = nullptr);
    void runAsync(QGraphicsScene* scene, NetManager* netManager = nullptr);
    void cancel();
    bool isRunning() const { return m_running; }

    // Results
    QList<DesignRuleViolation> violations() const { return m_violations; }
    RuleExecutionStats stats() const { return m_stats; }

    // ERC Matrix integration
    void setErcRules(const SchematicERCRules& rules);
    SchematicERCRules ercRules() const { return m_ercRules; }

    // Performance tuning
    void setCacheEnabled(bool enabled);
    bool isCacheEnabled() const { return m_cacheEnabled; }
    void clearCache();

    // Static helpers
    static int optimalThreadCount();
    static RuleEngineConfig defaultConfig();

Q_SIGNALS:
    // Execution state
    void executionStarted();
    void executionFinished(const QList<DesignRuleViolation>& violations);
    void executionCancelled();
    void executionError(const QString& error);

    // Progress
    void progressUpdated(const RuleProgress& progress);
    void ruleEvaluated(const QString& ruleName, int violationsFound);

    // Results
    void violationFound(const DesignRuleViolation& violation);
    void batchViolationsFound(const QList<DesignRuleViolation>& violations);

private Q_SLOTS:
    void onFutureFinished();
    void onProgressUpdated(int value);

private:
    // Internal execution
    void executeRules(QGraphicsScene* scene, NetManager* netManager);
    QList<DesignRuleViolation> evaluateRule(
        DesignRule* rule, 
        QGraphicsScene* scene, 
        NetManager* netManager
    );

    // ERC checks
    QList<DesignRuleViolation> checkErcRules(
        QGraphicsScene* scene, 
        NetManager* netManager
    );

    // DRC checks
    QList<DesignRuleViolation> checkDrcRules(
        QGraphicsScene* scene,
        NetManager* netManager
    );

    // Custom rule evaluation
    QList<DesignRuleViolation> evaluateCustomRule(
        DesignRule* rule,
        QGraphicsScene* scene,
        NetManager* netManager
    );

    // Caching
    QString getCacheKey(DesignRule* rule, QGraphicsScene* scene);
    bool getCachedResult(const QString& key, QList<DesignRuleViolation>& result);
    void cacheResult(const QString& key, const QList<DesignRuleViolation>& result);

    // Helpers
    void filterViolationsBySeverity(QList<DesignRuleViolation>& violations);
    void sortViolations(QList<DesignRuleViolation>& violations);

private:
    RuleEngineConfig m_config;
    DesignRuleSet* m_ruleSet;
    SchematicERCRules m_ercRules;

    // Execution state
    bool m_running;
    bool m_cancelled;
    QFuture<void> m_future;
    QFutureWatcher<void>* m_watcher;

    // Results
    QList<DesignRuleViolation> m_violations;
    RuleExecutionStats m_stats;
    QMutex m_violationsMutex;

    // Caching
    bool m_cacheEnabled;
    QHash<QString, QList<DesignRuleViolation>> m_resultCache;
    QMutex m_cacheMutex;

    // Threading
    QThreadPool* m_threadPool;
};

} // namespace Core
} // namespace Flux

#endif // DESIGN_RULE_ENGINE_H
