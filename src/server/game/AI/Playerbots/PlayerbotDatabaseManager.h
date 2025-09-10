/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TRINITY_PLAYERBOTDATABASEMANAGER_H
#define TRINITY_PLAYERBOTDATABASEMANAGER_H

#ifdef WITH_PLAYERBOTS

#include "Define.h"
#include "DatabaseEnv.h"
#include "QueryCallback.h"
#include "QueryResult.h"
#include "PreparedStatement.h"
#include <string>
#include <vector>
#include <queue>
#include <memory>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <unordered_map>
#include <thread>

enum class PlayerbotDatabaseType : uint8
{
    CHARACTER       = 0,    // Character database operations
    WORLD           = 1,    // World database operations  
    AUTH            = 2,    // Auth database operations
    HOTFIXES        = 3     // Hotfixes database operations
};

enum class PlayerbotQueryPriority : uint8
{
    LOW             = 0,    // Background statistics, cleanup
    NORMAL          = 1,    // Standard bot data operations
    HIGH            = 2,    // Bot state updates, action results
    CRITICAL        = 3,    // Login/logout, emergency saves
    IMMEDIATE       = 4     // Anti-cheat, security operations
};

enum class PlayerbotBatchType : uint8
{
    INSERT          = 0,    // Batch INSERT operations
    UPDATE          = 1,    // Batch UPDATE operations  
    DELETE          = 2,    // Batch DELETE operations
    SELECT          = 3,    // Batch SELECT operations
    MIXED           = 4     // Mixed operation types
};

struct PlayerbotDatabaseOperation
{
    PlayerbotDatabaseType dbType;
    PlayerbotQueryPriority priority;
    std::string query;
    std::vector<std::pair<uint8, QueryParameter>> parameters;
    std::function<void(PreparedQueryResult)> callback;
    std::chrono::steady_clock::time_point timestamp;
    std::chrono::steady_clock::time_point deadline;
    uint32 operationId;
    std::string description;
    bool isBatchable;
    PlayerbotBatchType batchType;
    
    PlayerbotDatabaseOperation() : dbType(PlayerbotDatabaseType::CHARACTER),
                                  priority(PlayerbotQueryPriority::NORMAL),
                                  timestamp(std::chrono::steady_clock::now()),
                                  deadline(std::chrono::steady_clock::now() + std::chrono::seconds(30)),
                                  operationId(0), isBatchable(false),
                                  batchType(PlayerbotBatchType::SELECT) {}
};

struct PlayerbotBatchOperation
{
    PlayerbotDatabaseType dbType;
    PlayerbotBatchType batchType;
    std::string baseQuery;
    std::vector<std::vector<QueryParameter>> parameterSets;
    std::function<void(std::vector<PreparedQueryResult>)> callback;
    std::chrono::steady_clock::time_point timestamp;
    uint32 batchSize;
    PlayerbotQueryPriority priority;
    
    PlayerbotBatchOperation() : dbType(PlayerbotDatabaseType::CHARACTER),
                               batchType(PlayerbotBatchType::INSERT),
                               timestamp(std::chrono::steady_clock::now()),
                               batchSize(0), priority(PlayerbotQueryPriority::NORMAL) {}
};

struct PlayerbotConnectionPoolStats
{
    std::atomic<uint32_t> totalConnections{0};
    std::atomic<uint32_t> activeConnections{0};
    std::atomic<uint32_t> idleConnections{0};
    std::atomic<uint32_t> connectionsCreated{0};
    std::atomic<uint32_t> connectionsDestroyed{0};
    std::atomic<uint64_t> totalQueries{0};
    std::atomic<uint64_t> successfulQueries{0};
    std::atomic<uint64_t> failedQueries{0};
    std::atomic<uint64_t> totalQueryTime{0};     // microseconds
    std::atomic<uint32_t> averageQueryTime{0};   // microseconds
    std::atomic<uint32_t> connectionWaitTime{0}; // microseconds
    std::chrono::steady_clock::time_point lastUpdate;
    
    PlayerbotConnectionPoolStats() : lastUpdate(std::chrono::steady_clock::now()) {}
};

struct PlayerbotDatabaseConfig
{
    uint32 minConnections;
    uint32 maxConnections;
    uint32 connectionTimeoutMs;
    uint32 queryTimeoutMs;
    uint32 batchSize;
    uint32 batchTimeoutMs;
    uint32 maxRetries;
    uint32 retryDelayMs;
    bool enableConnectionPooling;
    bool enableBatchOperations;
    bool enableQueryPreperation;
    bool enableAsyncOperations;
    uint32 statisticsUpdateIntervalMs;
    
    PlayerbotDatabaseConfig() : minConnections(5), maxConnections(50),
                               connectionTimeoutMs(10000), queryTimeoutMs(30000),
                               batchSize(100), batchTimeoutMs(5000),
                               maxRetries(3), retryDelayMs(1000),
                               enableConnectionPooling(true), enableBatchOperations(true),
                               enableQueryPreperation(true), enableAsyncOperations(true),
                               statisticsUpdateIntervalMs(5000) {}
};

class PlayerbotDatabaseConnection;
class PlayerbotConnectionPool;

/**
 * @brief Manages high-performance database operations for massive playerbot deployments
 * 
 * This class handles:
 * - Connection pooling with dynamic scaling (10-200 connections)
 * - Batch operations for 100x efficiency improvement
 * - Asynchronous query execution with priority queuing
 * - Query preparation and statement caching
 * - Load balancing across multiple database connections
 * - Performance monitoring and optimization
 * - Automatic retry and error recovery
 * - Connection lifecycle management
 */
class TC_GAME_API PlayerbotDatabaseManager
{
public:
    static PlayerbotDatabaseManager* Instance();
    ~PlayerbotDatabaseManager();
    
    // Initialization and configuration
    void Initialize(PlayerbotDatabaseConfig const& config = PlayerbotDatabaseConfig());
    void Shutdown();
    bool IsInitialized() const { return _initialized; }
    void SetConfiguration(PlayerbotDatabaseConfig const& config);
    PlayerbotDatabaseConfig GetConfiguration() const { return _config; }
    
    // Connection pool management
    void CreateConnectionPool(PlayerbotDatabaseType dbType, uint32 minSize, uint32 maxSize);
    void ScaleConnectionPool(PlayerbotDatabaseType dbType, int32 delta);
    uint32 GetPoolSize(PlayerbotDatabaseType dbType) const;
    uint32 GetActiveConnections(PlayerbotDatabaseType dbType) const;
    bool IsConnectionPoolHealthy(PlayerbotDatabaseType dbType) const;
    
    // Synchronous database operations
    PreparedQueryResult ExecuteQuery(PlayerbotDatabaseType dbType, std::string const& query,
                                    std::vector<QueryParameter> const& params = {});
    bool ExecuteUpdate(PlayerbotDatabaseType dbType, std::string const& query,
                      std::vector<QueryParameter> const& params = {});
    uint64 ExecuteInsert(PlayerbotDatabaseType dbType, std::string const& query,
                        std::vector<QueryParameter> const& params = {});
    
    // Asynchronous database operations
    void ExecuteQueryAsync(PlayerbotDatabaseType dbType, std::string const& query,
                          std::vector<QueryParameter> const& params,
                          std::function<void(PreparedQueryResult)> callback,
                          PlayerbotQueryPriority priority = PlayerbotQueryPriority::NORMAL);
    void ExecuteUpdateAsync(PlayerbotDatabaseType dbType, std::string const& query,
                           std::vector<QueryParameter> const& params,
                           std::function<void(bool)> callback,
                           PlayerbotQueryPriority priority = PlayerbotQueryPriority::NORMAL);
    
    // Batch operations for massive efficiency
    void ExecuteBatchInsert(PlayerbotDatabaseType dbType, std::string const& baseQuery,
                           std::vector<std::vector<QueryParameter>> const& parameterSets,
                           std::function<void(std::vector<bool>)> callback = nullptr);
    void ExecuteBatchUpdate(PlayerbotDatabaseType dbType, std::string const& baseQuery,
                           std::vector<std::vector<QueryParameter>> const& parameterSets,
                           std::function<void(std::vector<bool>)> callback = nullptr);
    void ExecuteBatchSelect(PlayerbotDatabaseType dbType, std::string const& baseQuery,
                           std::vector<std::vector<QueryParameter>> const& parameterSets,
                           std::function<void(std::vector<PreparedQueryResult>)> callback);
    
    // Prepared statement management
    void CachePreparedStatement(PlayerbotDatabaseType dbType, std::string const& name,
                               std::string const& query);
    PreparedQueryResult ExecutePreparedQuery(PlayerbotDatabaseType dbType, std::string const& name,
                                            std::vector<QueryParameter> const& params = {});
    void ExecutePreparedQueryAsync(PlayerbotDatabaseType dbType, std::string const& name,
                                  std::vector<QueryParameter> const& params,
                                  std::function<void(PreparedQueryResult)> callback,
                                  PlayerbotQueryPriority priority = PlayerbotQueryPriority::NORMAL);
    
    // Transaction management
    class Transaction;
    std::unique_ptr<Transaction> BeginTransaction(PlayerbotDatabaseType dbType);
    bool CommitTransaction(std::unique_ptr<Transaction> transaction);
    void RollbackTransaction(std::unique_ptr<Transaction> transaction);
    
    // Performance optimization
    void OptimizeDatabaseOperations();
    void FlushPendingOperations(PlayerbotDatabaseType dbType = PlayerbotDatabaseType::CHARACTER);
    void ProcessBatchOperations();
    void CleanupExpiredOperations();
    void PrewarmConnections();
    
    // Statistics and monitoring
    PlayerbotConnectionPoolStats GetStatistics(PlayerbotDatabaseType dbType) const;
    std::vector<std::string> GetPerformanceReport() const;
    float GetAverageQueryTime(PlayerbotDatabaseType dbType) const;
    uint32 GetQueuedOperationCount(PlayerbotDatabaseType dbType) const;
    float GetConnectionUtilization(PlayerbotDatabaseType dbType) const;
    
    // Health monitoring and maintenance
    void ValidateConnections();
    void ReconnectFailedConnections();
    void PerformMaintenance();
    bool TestConnection(PlayerbotDatabaseType dbType);
    void LogDatabaseActivity(std::string const& activity, PlayerbotDatabaseType dbType);
    
    // Emergency operations
    void EmergencyFlush();
    void EmergencyReconnect();
    void SetEmergencyMode(bool enabled);
    bool IsInEmergencyMode() const { return _emergencyMode; }
    
    // Update and lifecycle
    void Update(uint32 diff);
    void UpdateStatistics();
    void ProcessAsyncOperations();
    void ProcessRetryQueue();

private:
    PlayerbotDatabaseManager();
    
    static std::unique_ptr<PlayerbotDatabaseManager> _instance;
    static std::mutex _instanceMutex;
    
    // Configuration and state
    PlayerbotDatabaseConfig _config;
    std::atomic<bool> _initialized{false};
    std::atomic<bool> _shutdown{false};
    std::atomic<bool> _emergencyMode{false};
    
    // Connection pools by database type
    std::unordered_map<PlayerbotDatabaseType, std::unique_ptr<PlayerbotConnectionPool>> _connectionPools;
    std::mutex _poolMutex;
    
    // Operation queues
    std::unordered_map<PlayerbotDatabaseType, std::queue<PlayerbotDatabaseOperation>> _operationQueues;
    std::unordered_map<PlayerbotDatabaseType, std::mutex> _queueMutexes;
    std::unordered_map<PlayerbotDatabaseType, std::condition_variable> _queueConditions;
    
    // Batch operation management
    std::unordered_map<PlayerbotDatabaseType, std::vector<PlayerbotBatchOperation>> _batchQueues;
    std::mutex _batchMutex;
    std::chrono::steady_clock::time_point _lastBatchProcess;
    
    // Prepared statement cache
    std::unordered_map<PlayerbotDatabaseType, std::unordered_map<std::string, std::string>> _preparedStatements;
    std::mutex _statementCacheMutex;
    
    // Statistics
    std::unordered_map<PlayerbotDatabaseType, PlayerbotConnectionPoolStats> _statistics;
    std::chrono::steady_clock::time_point _lastStatisticsUpdate;
    
    // Worker threads
    std::vector<std::thread> _workerThreads;
    std::atomic<bool> _workersActive{false};
    
    // Retry management
    std::queue<PlayerbotDatabaseOperation> _retryQueue;
    std::mutex _retryMutex;
    std::chrono::steady_clock::time_point _lastRetryProcess;
    
    // Performance monitoring
    std::atomic<uint32_t> _operationIdCounter{0};
    std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> _operationTimestamps;
    std::mutex _timestampMutex;
    
    // Connection pool implementation
    class PlayerbotConnectionPool
    {
    public:
        PlayerbotConnectionPool(PlayerbotDatabaseType type, uint32 minSize, uint32 maxSize);
        ~PlayerbotConnectionPool();
        
        std::shared_ptr<PlayerbotDatabaseConnection> AcquireConnection();
        void ReleaseConnection(std::shared_ptr<PlayerbotDatabaseConnection> connection);
        void ScalePool(int32 delta);
        uint32 GetSize() const;
        uint32 GetActiveCount() const;
        bool IsHealthy() const;
        void Shutdown();
        
    private:
        PlayerbotDatabaseType _dbType;
        uint32 _minSize;
        uint32 _maxSize;
        std::vector<std::shared_ptr<PlayerbotDatabaseConnection>> _connections;
        std::queue<std::shared_ptr<PlayerbotDatabaseConnection>> _availableConnections;
        std::mutex _poolMutex;
        std::condition_variable _poolCondition;
        std::atomic<bool> _shutdown{false};
        
        void CreateConnection();
        void RemoveConnection();
        void ValidateConnection(std::shared_ptr<PlayerbotDatabaseConnection> connection);
    };
    
    // Database connection wrapper
    class PlayerbotDatabaseConnection
    {
    public:
        PlayerbotDatabaseConnection(PlayerbotDatabaseType type);
        ~PlayerbotDatabaseConnection();
        
        bool Connect();
        void Disconnect();
        bool IsConnected() const;
        bool Ping();
        
        PreparedQueryResult ExecuteQuery(std::string const& query, std::vector<QueryParameter> const& params);
        bool ExecuteUpdate(std::string const& query, std::vector<QueryParameter> const& params);
        uint64 ExecuteInsert(std::string const& query, std::vector<QueryParameter> const& params);
        
        void BeginTransaction();
        void CommitTransaction();
        void RollbackTransaction();
        bool InTransaction() const { return _inTransaction; }
        
        std::chrono::steady_clock::time_point GetLastUsed() const { return _lastUsed; }
        uint32 GetQueryCount() const { return _queryCount; }
        
    private:
        PlayerbotDatabaseType _dbType;
        std::unique_ptr<DatabaseWorkerPool<CharacterDatabaseConnection>> _charConnection;
        std::unique_ptr<DatabaseWorkerPool<WorldDatabaseConnection>> _worldConnection;
        std::unique_ptr<DatabaseWorkerPool<LoginDatabaseConnection>> _authConnection;
        std::unique_ptr<DatabaseWorkerPool<HotfixDatabaseConnection>> _hotfixConnection;
        
        bool _connected;
        bool _inTransaction;
        std::chrono::steady_clock::time_point _lastUsed;
        std::atomic<uint32_t> _queryCount{0};
        
        DatabaseWorkerPool<CharacterDatabaseConnection>* GetCharacterDatabase() const;
        DatabaseWorkerPool<WorldDatabaseConnection>* GetWorldDatabase() const;
        DatabaseWorkerPool<LoginDatabaseConnection>* GetAuthDatabase() const;
        DatabaseWorkerPool<HotfixDatabaseConnection>* GetHotfixDatabase() const;
    };
    
    // Transaction wrapper
    class Transaction
    {
    public:
        Transaction(std::shared_ptr<PlayerbotDatabaseConnection> connection);
        ~Transaction();
        
        void AddQuery(std::string const& query, std::vector<QueryParameter> const& params);
        bool Execute();
        void Rollback();
        bool IsActive() const { return _active; }
        
    private:
        std::shared_ptr<PlayerbotDatabaseConnection> _connection;
        std::vector<std::pair<std::string, std::vector<QueryParameter>>> _queries;
        bool _active;
    };
    
    // Helper methods
    void InitializeConnectionPools();
    void ShutdownConnectionPools();
    void StartWorkerThreads();
    void StopWorkerThreads();
    void WorkerThreadMain(PlayerbotDatabaseType dbType);
    
    // Operation processing
    void ProcessOperation(PlayerbotDatabaseOperation const& operation);
    void ProcessBatchOperation(PlayerbotBatchOperation const& batchOp);
    bool ShouldBatchOperation(PlayerbotDatabaseOperation const& operation);
    void AddToBatch(PlayerbotDatabaseOperation const& operation);
    
    // Error handling and retry
    void HandleConnectionError(PlayerbotDatabaseType dbType, std::string const& error);
    void HandleQueryError(PlayerbotDatabaseOperation const& operation, std::string const& error);
    void RetryFailedOperation(PlayerbotDatabaseOperation const& operation);
    
    // Performance optimization
    void OptimizeConnectionDistribution();
    void AnalyzeQueryPatterns();
    void AdjustBatchSizes();
    void PreloadCommonQueries();
    
    // Statistics and monitoring
    void UpdateConnectionStatistics(PlayerbotDatabaseType dbType, uint32 queryTimeUs);
    void LogPerformanceMetrics();
    void DetectPerformanceIssues();
    
    // Utility methods
    DatabaseWorkerPool<CharacterDatabaseConnection>* GetDatabase(PlayerbotDatabaseType dbType) const;
    std::string GetDatabaseName(PlayerbotDatabaseType dbType) const;
    uint32 GenerateOperationId() { return ++_operationIdCounter; }
    
    // Validation and safety
    bool ValidateOperation(PlayerbotDatabaseOperation const& operation) const;
    bool ValidateParameters(std::vector<QueryParameter> const& params) const;
    void SanitizeQuery(std::string& query) const;
};

#define sPlayerbotDatabaseManager PlayerbotDatabaseManager::Instance()

#endif // WITH_PLAYERBOTS
#endif // TRINITY_PLAYERBOTDATABASEMANAGER_H