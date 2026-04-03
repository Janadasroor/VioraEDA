#ifndef PCB_AUTO_ROUTER_H
#define PCB_AUTO_ROUTER_H

#include <QObject>
#include <QGraphicsScene>
#include <QVector>
#include <QMap>
#include <QSet>
#include <QPointF>
#include <QString>
#include <QSize>
#include <functional>

class TraceItem;
class ViaItem;
class PadItem;
class PCBItem;

/**
 * @brief Grid-based A* auto-router with rip-up and retry strategy.
 *
 * Algorithm overview:
 * 1. Build a routing grid from the board bounding box
 * 2. Mark obstacles (pads, existing traces, keepouts) on each layer
 * 3. For each unrouted net (from ratsnest):
 *    a. Run A* pathfinding on the grid (supports layer changes via vias)
 *    b. If path found: convert grid path to trace segments + vias
 *    c. If no path: mark as failed for rip-up retry
 * 4. Rip-up failed nets, retry with more aggressive costs
 * 5. Report statistics
 *
 * Supports:
 * - Multi-layer routing with automatic via insertion
 * - DRC-aware (clearance, trace width)
 * - Existing trace preservation (routes around pre-routed traces)
 * - Progress callbacks
 * - Rip-up and retry for difficult connections
 */
class PCBAutoRouter : public QObject {
    Q_OBJECT

public:
    struct RouterConfig {
        // Grid settings
        double gridSpacing = 0.5;        // Grid cell size in mm (default 0.5mm)
        int maxGridWidth = 2000;          // Max grid width to limit memory
        int maxGridHeight = 2000;         // Max grid height

        // Routing parameters
        int maxIterations = 50000;        // Max A* iterations per connection
        int maxRipUpRounds = 3;           // Max rip-up and retry rounds
        int maxRipUpCount = 100;          // Max traces to rip up per round

        // DRC parameters
        double clearance = 0.2;           // Minimum clearance in mm
        double viaClearance = 0.3;        // Via clearance in mm
        int maxViasPerNet = 10;           // Maximum vias per connection
        bool preferTopLayer = true;       // Prefer routing on top layer
        bool preferBottomLayer = true;    // Also use bottom layer
        bool allowDiagonals = false;      // Allow 45-degree diagonal moves
        bool optimizeTraceLength = true;  // Prioritize shorter traces

        // Progress
        bool reportProgress = true;
        int progressInterval = 100;       // Report progress every N connections
    };

    struct RouteStats {
        int totalConnections = 0;
        int routedConnections = 0;
        int failedConnections = 0;
        int ripUpCount = 0;
        int viaCount = 0;
        int traceSegmentCount = 0;
        double totalTraceLength = 0.0;
        QStringList failedNets;           // Net names that couldn't be routed
        int iterations = 0;
    };

    enum class RoutingPass {
        Initial,        // First pass - no rip-up
        RipUpRetry,     // Second pass - with rip-up
        Final           // Final aggressive pass
    };

    explicit PCBAutoRouter(QGraphicsScene* scene, QObject* parent = nullptr);
    ~PCBAutoRouter();

    /**
     * @brief Run the auto-router on all unrouted connections.
     * @param config Routing configuration
     * @return Statistics about the routing result
     */
    RouteStats routeAll(const RouterConfig& config);

    /**
     * @brief Route a single net by name.
     * @return true if successfully routed
     */
    bool routeNet(const QString& netName, const RouterConfig& config);

    /**
     * @brief Check if routing is currently in progress.
     */
    bool isRunning() const { return m_running; }

    /**
     * @brief Request the router to stop gracefully.
     */
    void requestStop() { m_stopRequested = true; }

    /**
     * @brief Get the current routing progress (0.0 to 1.0).
     */
    double progress() const;

signals:
    /** Emitted when a connection is successfully routed. */
    void connectionRouted(const QString& netName, int current, int total);

    /** Emitted when a connection fails to route. */
    void connectionFailed(const QString& netName, int current, int total);

    /** Emitted periodically with overall progress. */
    void progressChanged(double progress, const QString& status);

    /** Emitted when routing is complete (success or failure). */
    void routingFinished(const RouteStats& stats);

private:
    // Grid representation
    struct GridCell {
        bool blocked = false;            // Obstacle (pad, copper pour, keepout)
        bool traceOccupied = false;      // Existing trace (route around)
        double occupancyCost = 0.0;      // Cost penalty for using this cell
        int layer = 0;                   // Which copper layer this cell belongs to
    };

    // A* node
    struct AStarNode {
        int x, y;                         // Grid coordinates
        int layer;                        // Layer index
        double gCost = 0.0;              // Cost from start
        double hCost = 0.0;              // Heuristic cost to goal
        double fCost() const { return gCost + hCost; }
        int parentX = -1, parentY = -1;  // Parent node
        int parentLayer = -1;
        bool isVia = false;              // True if this node transitions layers
    };

    struct UnroutedConnection {
        QString netName;
        QPointF start;    // Start pad position
        QPointF end;      // End pad position
        double startClearance = 0.2;
        double traceWidth = 0.25;
    };

    // Grid management
    void buildGrid();
    void markObstacles();
    void markExistingTraces();
    GridCell* cellAt(int x, int y, int layer);
    const GridCell* cellAt(int x, int y, int layer) const;
    bool isValidCell(int x, int y, int layer) const;

    // Coordinate conversion
    QPointF gridToScene(int gx, int gy) const;
    QPoint sceneToGrid(QPointF scenePos) const;

    // A* pathfinding
    bool findPath(const UnroutedConnection& conn, QVector<AStarNode>& outPath);
    double heuristic(int x1, int y1, int x2, int y2) const;
    bool isCellPassable(int x, int y, int layer, double clearance) const;
    QList<AStarNode> getNeighbors(const AStarNode& node, double clearance) const;

    // Path conversion
    void convertPathToTraces(const QVector<AStarNode>& path, const UnroutedConnection& conn);
    TraceItem* createTraceSegment(QPointF start, QPointF end, int layer, const QString& netName, double width);
    ViaItem* createVia(QPointF pos, int startLayer, int endLayer, const QString& netName);

    // Rip-up and retry
    int ripUpWorstTraces(int count, const QSet<QString>& failedNets);

    // Connection discovery
    QList<UnroutedConnection> findUnroutedConnections();
    QMultiMap<QString, QPointF> groupPadsByNet();
    bool arePadsConnected(QPointF p1, QPointF p2, const QString& netName) const;

    // Statistics
    void updateStats();

    QGraphicsScene* m_scene;
    RouterConfig m_config;
    RouteStats m_stats;
    bool m_running = false;
    bool m_stopRequested = false;

    // Grid
    QVector<GridCell> m_grid;             // Flat grid: [layer][y][x]
    int m_gridWidth = 0;
    int m_gridHeight = 0;
    int m_gridLayers = 2;                 // Top and bottom copper
    double m_gridOriginX = 0.0;
    double m_gridOriginY = 0.0;

    // Progress tracking
    int m_currentConnection = 0;
    int m_totalConnections = 0;

    // A* open/closed sets (using vectors for performance)
    struct AStarOpenEntry {
        double fCost;
        int x, y, layer;
        bool operator>(const AStarOpenEntry& other) const { return fCost > other.fCost; }
    };
};

#endif // PCB_AUTO_ROUTER_H
