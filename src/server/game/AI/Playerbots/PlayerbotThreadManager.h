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

#ifndef TRINITY_PLAYERBOTTHREADMANAGER_H
#define TRINITY_PLAYERBOTTHREADMANAGER_H

#ifdef WITH_PLAYERBOTS

#include "Define.h"
#include "ObjectGuid.h"
#include <string>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <functional>
#include <chrono>
#include <future>

class PlayerbotPlayerAI;

enum class PlayerbotTaskPriority : uint8
{
    IDLE            = 0,    // Background maintenance tasks
    LOW             = 1,    // Non-critical updates
    NORMAL          = 2,    // Standard AI processing
    HIGH            = 3,    // Combat and reactive behaviors
    CRITICAL        = 4,    // Safety and emergency responses
    IMMEDIATE       = 5     // Must execute immediately
};

enum class PlayerbotTaskType : uint8
{
    AI_UPDATE       = 0,    // Regular AI decision making
    ACTION_EXECUTE  = 1,    // Execute specific actions
    DATA_SYNC       = 2,    // Database synchronization
    CACHE_UPDATE    = 3,    // Cache maintenance
    PERFORMANCE     = 4,    // Performance monitoring
    MAINTENANCE     = 5,    // System maintenance
    EMERGENCY       = 6     // Emergency responses
};

enum class PlayerbotThreadType : uint8
{
    MAIN_AI         = 0,    // Primary AI processing threads
    ACTION_WORKER   = 1,    // Action execution threads
    DATABASE        = 2,    // Database operation threads
    CACHE_MANAGER   = 3,    // Cache maintenance threads
    MONITOR         = 4,    // Performance monitoring thread
    MAINTENANCE     = 5     // Background maintenance thread
};

struct PlayerbotTask
{
    ObjectGuid botGuid;
    PlayerbotPlayerAI* ai;
    PlayerbotTaskType type;
    PlayerbotTaskPriority priority;
    std::function<void()> task;
    std::chrono::steady_clock::time_point createdTime;
    std::chrono::steady_clock::time_point deadline;
    uint32 estimatedDuration;  // microseconds
    std::string description;
    bool repeating;
    uint32 repeatInterval;  // milliseconds
    
    PlayerbotTask() : ai(nullptr), type(PlayerbotTaskType::AI_UPDATE),
                     priority(PlayerbotTaskPriority::NORMAL),
                     createdTime(std::chrono::steady_clock::now()),
                     deadline(std::chrono::steady_clock::now() + std::chrono::seconds(5)),
                     estimatedDuration(1000), repeating(false), repeatInterval(0) {}
    
    PlayerbotTask(ObjectGuid guid, PlayerbotPlayerAI* botAI, PlayerbotTaskType taskType,
                 PlayerbotTaskPriority prio, std::function<void()> func,
                 std::string desc = "", uint32 duration = 1000) :
        botGuid(guid), ai(botAI), type(taskType), priority(prio), task(func),
        createdTime(std::chrono::steady_clock::now()),
        deadline(std::chrono::steady_clock::now() + std::chrono::seconds(5)),
        estimatedDuration(duration), description(desc), repeating(false), repeatInterval(0) {}
};

struct PlayerbotTaskComparator
{
    bool operator()(const PlayerbotTask& lhs, const PlayerbotTask& rhs) const
    {
        // Higher priority first
        if (lhs.priority != rhs.priority)
            return lhs.priority < rhs.priority;
        
        // Earlier deadline first for same priority
        return lhs.deadline > rhs.deadline;
    }
};

struct PlayerbotThreadStatistics
{
    std::atomic<uint64_t> tasksProcessed{0};
    std::atomic<uint64_t> tasksQueued{0};
    std::atomic<uint64_t> totalExecutionTime{0};    // microseconds
    std::atomic<uint32_t> averageExecutionTime{0};   // microseconds
    std::atomic<uint32_t> queueSize{0};
    std::atomic<uint32_t> activeThreads{0};
    std::atomic<uint32_t> idleThreads{0};
    std::chrono::steady_clock::time_point lastUpdate;
    
    PlayerbotThreadStatistics() : lastUpdate(std::chrono::steady_clock::now()) {}
};

struct PlayerbotThreadPoolConfig
{
    uint32 minThreads;
    uint32 maxThreads;
    uint32 queueSizeLimit;
    uint32 taskTimeoutMs;
    uint32 threadIdleTimeoutMs;
    bool enableCPUAffinity;
    bool enablePriorityScheduling;
    uint32 performanceUpdateIntervalMs;
    
    PlayerbotThreadPoolConfig() : minThreads(8), maxThreads(64), queueSizeLimit(100000),
                                 taskTimeoutMs(3000), threadIdleTimeoutMs(15000),
                                 enableCPUAffinity(true), enablePriorityScheduling(true),
                                 performanceUpdateIntervalMs(500) {}
};

/**
 * @brief Manages multi-threaded execution of playerbot AI operations
 * 
 * This class handles:
 * - Thread pool management with dynamic scaling
 * - Priority-based task scheduling and execution
 * - CPU affinity optimization for performance
 * - Lock-free inter-thread communication where possible
 * - Performance monitoring and bottleneck detection
 * - Resource contention prevention and deadlock avoidance
 * - Thread-safe coordination with game systems
 */
class TC_GAME_API PlayerbotThreadManager
{
public:
    static PlayerbotThreadManager* Instance();
    
    ~PlayerbotThreadManager();

    // Initialization and configuration
    void Initialize(PlayerbotThreadPoolConfig const& config = PlayerbotThreadPoolConfig());
    void Shutdown();
    bool IsInitialized() const { return _initialized; }
    void SetConfiguration(PlayerbotThreadPoolConfig const& config);
    PlayerbotThreadPoolConfig GetConfiguration() const { return _config; }
    
    // Task scheduling and execution
    void ScheduleTask(PlayerbotTask const& task);
    void ScheduleRepeatingTask(PlayerbotTask const& task, uint32 intervalMs);
    void ScheduleDelayedTask(PlayerbotTask const& task, uint32 delayMs);
    template<typename F, typename... Args>
    auto ScheduleTaskWithResult(PlayerbotTaskPriority priority, F&& func, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;
    
    // Task management
    void CancelTasks(ObjectGuid botGuid);
    void CancelTasksByType(PlayerbotTaskType type);
    uint32 GetQueuedTaskCount() const { return _statistics.queueSize.load(); }
    uint32 GetQueuedTaskCount(PlayerbotTaskPriority priority) const;
    bool HasPendingTasks(ObjectGuid botGuid) const;
    
    // Thread pool management
    void SetThreadCount(PlayerbotThreadType type, uint32 count);
    uint32 GetThreadCount(PlayerbotThreadType type) const;
    void ScaleThreadPool(int32 delta);
    void OptimizeThreadAllocation();
    bool IsThreadPoolHealthy() const;
    
    // Performance monitoring
    PlayerbotThreadStatistics GetStatistics() const { return _statistics; }
    float GetAverageCPUUsage() const;
    uint32 GetAverageQueueSize() const;
    float GetThroughput() const; // tasks per second
    std::vector<std::string> GetPerformanceReport() const;
    
    // Thread synchronization
    void WaitForCompletion(uint32 timeoutMs = 5000);
    void WaitForTask(ObjectGuid botGuid, uint32 timeoutMs = 1000);
    void SynchronizeAllBots();
    void EmergencyStop();
    
    // CPU affinity and optimization
    void SetCPUAffinity(std::vector<uint32> const& cpuCores);
    void EnableNUMAAwareness(bool enable) { _numaEnabled = enable; }
    void OptimizeCacheLocality();
    uint32 GetOptimalThreadCount() const;
    
    // Update and maintenance
    void Update(uint32 diff);
    void UpdatePerformanceMetrics();
    void CleanupCompletedTasks();
    void DefragmentQueues();

private:
    PlayerbotThreadManager();
    
    static std::unique_ptr<PlayerbotThreadManager> _instance;
    static std::mutex _instanceMutex;
    
    // Configuration and state
    PlayerbotThreadPoolConfig _config;
    std::atomic<bool> _initialized{false};
    std::atomic<bool> _shutdown{false};
    std::atomic<bool> _emergencyStop{false};
    
    // Thread pools by type
    std::map<PlayerbotThreadType, std::vector<std::thread>> _threadPools;
    std::map<PlayerbotThreadType, std::atomic<bool>> _threadPoolActive;
    
    // Task queues (priority queues for each thread type)
    std::map<PlayerbotThreadType, std::priority_queue<PlayerbotTask, std::vector<PlayerbotTask>, PlayerbotTaskComparator>> _taskQueues;
    std::map<PlayerbotThreadType, std::mutex> _queueMutexes;
    std::map<PlayerbotThreadType, std::condition_variable> _queueConditions;
    
    // Thread synchronization
    std::mutex _configMutex;
    std::mutex _statisticsMutex;
    std::condition_variable _shutdownCondition;
    
    // Performance monitoring
    PlayerbotThreadStatistics _statistics;
    std::chrono::steady_clock::time_point _lastPerformanceUpdate;
    std::atomic<float> _averageCPUUsage{0.0f};
    std::atomic<uint32_t> _averageQueueSize{0};
    
    // CPU optimization
    std::vector<uint32> _availableCPUCores;
    bool _numaEnabled;
    std::map<PlayerbotThreadType, std::vector<uint32>> _threadCPUAffinity;
    
    // Repeating tasks
    std::vector<PlayerbotTask> _repeatingTasks;
    std::mutex _repeatingTasksMutex;
    std::chrono::steady_clock::time_point _lastRepeatingTaskCheck;
    
    // Emergency handling
    std::atomic<uint32_t> _emergencyTaskCount{0};
    std::mutex _emergencyMutex;
    std::condition_variable _emergencyCondition;
    
    // Thread management
    void InitializeThreadPool(PlayerbotThreadType type, uint32 threadCount);
    void ShutdownThreadPool(PlayerbotThreadType type);
    void WorkerThread(PlayerbotThreadType type, uint32 threadIndex);
    void MonitorThread();
    void MaintenanceThread();
    
    // Task execution
    bool ExecuteTask(PlayerbotTask const& task);
    void HandleTaskTimeout(PlayerbotTask const& task);
    void HandleTaskException(PlayerbotTask const& task, std::exception const& ex);
    bool ValidateTask(PlayerbotTask const& task) const;
    
    // Performance optimization
    void DetectBottlenecks();
    void OptimizeTaskDistribution();
    void BalanceThreadLoads();
    PlayerbotThreadType GetOptimalThreadType(PlayerbotTask const& task) const;
    
    // CPU and memory optimization
    void SetThreadAffinity(std::thread::native_handle_type handle, std::vector<uint32> const& cpuCores);
    void OptimizeMemoryLayout();
    uint32 DetectCPUCores() const;
    std::vector<uint32> GetNUMANodes() const;
    
    // Queue management
    void ProcessTaskQueue(PlayerbotThreadType type);
    void RebalanceQueues();
    void PurgeExpiredTasks();
    uint32 GetTaskQueueSize(PlayerbotThreadType type) const;
    
    // Repeating task management
    void ProcessRepeatingTasks();
    void ScheduleNextRepeat(PlayerbotTask const& task);
    void CleanupRepeatingTasks();
    
    // Emergency handling
    void ProcessEmergencyTasks();
    void EscalateStuckTasks();
    void RecoverFromDeadlock();
    
    // Statistics and reporting
    void UpdateTaskStatistics(PlayerbotTask const& task, uint32 executionTime);
    void UpdateThreadStatistics();
    void GeneratePerformanceReport();
    
    // Thread safety utilities
    template<typename F>
    auto WithQueueLock(PlayerbotThreadType type, F&& func) -> decltype(func());
    bool TryLockQueue(PlayerbotThreadType type, uint32 timeoutMs);
    void SafeShutdown();
    
    // Validation and error handling
    bool ValidateConfiguration(PlayerbotThreadPoolConfig const& config) const;
    void HandleCriticalError(std::string const& error);
    void LogThreadActivity(std::string const& activity, PlayerbotThreadType type = PlayerbotThreadType::MAIN_AI);
};

// Template implementation for task scheduling with result
template<typename F, typename... Args>
auto PlayerbotThreadManager::ScheduleTaskWithResult(PlayerbotTaskPriority priority, F&& func, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type>
{
    using ReturnType = typename std::result_of<F(Args...)>::type;
    
    auto taskPromise = std::make_shared<std::promise<ReturnType>>();
    auto future = taskPromise->get_future();
    
    PlayerbotTask task;
    task.priority = priority;
    task.type = PlayerbotTaskType::ACTION_EXECUTE;
    task.description = "Task with result";
    
    if constexpr (std::is_void_v<ReturnType>)
    {
        task.task = [=]() {
            try {
                func(args...);
                taskPromise->set_value();
            }
            catch (...) {
                taskPromise->set_exception(std::current_exception());
            }
        };
    }
    else
    {
        task.task = [=]() {
            try {
                auto result = func(args...);
                taskPromise->set_value(result);
            }
            catch (...) {
                taskPromise->set_exception(std::current_exception());
            }
        };
    }
    
    ScheduleTask(task);
    return future;
}

#define sPlayerbotThreadManager PlayerbotThreadManager::Instance()

#endif // WITH_PLAYERBOTS
#endif // TRINITY_PLAYERBOTTHREADMANAGER_H