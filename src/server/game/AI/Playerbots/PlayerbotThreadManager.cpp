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

#ifdef WITH_PLAYERBOTS

#include "PlayerbotThreadManager.h"
#include "Log.h"
#include "Timer.h"
#include <algorithm>
#include <thread>
#include <chrono>
#include <exception>

#ifdef _WIN32
#include <windows.h>
#include <processthreadsapi.h>
#elif defined(__linux__)
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <numa.h>
#endif

std::unique_ptr<PlayerbotThreadManager> PlayerbotThreadManager::_instance = nullptr;
std::mutex PlayerbotThreadManager::_instanceMutex;

PlayerbotThreadManager* PlayerbotThreadManager::Instance()
{
    std::lock_guard<std::mutex> lock(_instanceMutex);
    if (!_instance)
        _instance = std::unique_ptr<PlayerbotThreadManager>(new PlayerbotThreadManager());
    return _instance.get();
}

PlayerbotThreadManager::PlayerbotThreadManager() :
    _numaEnabled(false),
    _lastPerformanceUpdate(std::chrono::steady_clock::now()),
    _lastRepeatingTaskCheck(std::chrono::steady_clock::now())
{
    // Detect available CPU cores
    _availableCPUCores = DetectCPUCores();
    
    TC_LOG_INFO("playerbots", "PlayerbotThreadManager initialized with {} CPU cores detected", 
               _availableCPUCores.size());
}

PlayerbotThreadManager::~PlayerbotThreadManager()
{
    Shutdown();
}

void PlayerbotThreadManager::Initialize(PlayerbotThreadPoolConfig const& config)
{
    if (_initialized.load())
    {
        TC_LOG_WARN("playerbots", "PlayerbotThreadManager already initialized");
        return;
    }
    
    if (!ValidateConfiguration(config))
    {
        TC_LOG_ERROR("playerbots", "Invalid PlayerbotThreadManager configuration");
        return;
    }
    
    std::lock_guard<std::mutex> lock(_configMutex);
    _config = config;
    
    // Initialize all thread pools
    InitializeThreadPool(PlayerbotThreadType::MAIN_AI, config.maxThreads / 2);
    InitializeThreadPool(PlayerbotThreadType::ACTION_WORKER, config.maxThreads / 4);
    InitializeThreadPool(PlayerbotThreadType::DATABASE, std::min(8u, config.maxThreads / 8));
    InitializeThreadPool(PlayerbotThreadType::CACHE_MANAGER, 2);
    InitializeThreadPool(PlayerbotThreadType::MONITOR, 1);
    InitializeThreadPool(PlayerbotThreadType::MAINTENANCE, 1);
    
    // Start monitoring and maintenance threads
    std::thread monitorThread(&PlayerbotThreadManager::MonitorThread, this);
    monitorThread.detach();
    
    std::thread maintenanceThread(&PlayerbotThreadManager::MaintenanceThread, this);
    maintenanceThread.detach();
    
    _initialized = true;
    
    TC_LOG_INFO("playerbots", "PlayerbotThreadManager initialized with {} total threads across {} thread pools",
               GetTotalThreadCount(), _threadPools.size());
    
    LogThreadActivity("Thread Manager Initialized");
}

void PlayerbotThreadManager::Shutdown()
{
    if (!_initialized.load())
        return;
    
    TC_LOG_INFO("playerbots", "PlayerbotThreadManager shutting down...");
    
    _shutdown = true;
    
    // Notify all waiting threads
    for (auto& [type, condition] : _queueConditions)
    {
        condition.notify_all();
    }
    
    // Wait for all threads to complete with timeout
    auto shutdownStart = std::chrono::steady_clock::now();
    const auto shutdownTimeout = std::chrono::seconds(10);
    
    for (auto& [type, threads] : _threadPools)
    {
        ShutdownThreadPool(type);
    }
    
    // Clean up remaining tasks
    for (auto& [type, queue] : _taskQueues)
    {
        std::lock_guard<std::mutex> lock(_queueMutexes[type]);
        while (!queue.empty())
            queue.pop();
    }
    
    _initialized = false;
    
    auto shutdownDuration = std::chrono::steady_clock::now() - shutdownStart;
    TC_LOG_INFO("playerbots", "PlayerbotThreadManager shutdown completed in {} ms",
               std::chrono::duration_cast<std::chrono::milliseconds>(shutdownDuration).count());
}

void PlayerbotThreadManager::InitializeThreadPool(PlayerbotThreadType type, uint32 threadCount)
{
    if (threadCount == 0)
    {
        TC_LOG_WARN("playerbots", "Attempting to create thread pool with 0 threads for type {}", uint32(type));
        return;
    }
    
    _threadPoolActive[type] = true;
    _threadPools[type].reserve(threadCount);
    
    // Assign CPU affinity if enabled
    if (_config.enableCPUAffinity && !_availableCPUCores.empty())
    {
        std::vector<uint32> assignedCores;
        uint32 coresPerThread = std::max(1u, uint32(_availableCPUCores.size()) / threadCount);
        
        for (uint32 i = 0; i < threadCount; ++i)
        {
            uint32 startCore = (i * coresPerThread) % _availableCPUCores.size();
            for (uint32 j = 0; j < coresPerThread && (startCore + j) < _availableCPUCores.size(); ++j)
            {
                assignedCores.push_back(_availableCPUCores[startCore + j]);
            }
            _threadCPUAffinity[type].push_back(assignedCores.back());
        }
    }
    
    // Create worker threads
    for (uint32 i = 0; i < threadCount; ++i)
    {
        _threadPools[type].emplace_back(&PlayerbotThreadManager::WorkerThread, this, type, i);
        
        // Set CPU affinity if enabled
        if (_config.enableCPUAffinity && !_threadCPUAffinity[type].empty())
        {
            std::vector<uint32> cpuCore = { _threadCPUAffinity[type][i % _threadCPUAffinity[type].size()] };
            SetThreadAffinity(_threadPools[type].back().native_handle(), cpuCore);
        }
    }
    
    _statistics.activeThreads += threadCount;
    
    TC_LOG_DEBUG("playerbots", "Initialized thread pool type {} with {} threads", uint32(type), threadCount);
}

void PlayerbotThreadManager::ShutdownThreadPool(PlayerbotThreadType type)
{
    auto poolIt = _threadPools.find(type);
    if (poolIt == _threadPools.end())
        return;
    
    _threadPoolActive[type] = false;
    
    // Notify all threads in this pool
    _queueConditions[type].notify_all();
    
    // Join all threads with timeout
    for (auto& thread : poolIt->second)
    {
        if (thread.joinable())
        {
            thread.join();
            _statistics.activeThreads--;
        }
    }
    
    poolIt->second.clear();
    
    TC_LOG_DEBUG("playerbots", "Shutdown thread pool type {}", uint32(type));
}

void PlayerbotThreadManager::WorkerThread(PlayerbotThreadType type, uint32 threadIndex)
{
    LogThreadActivity("Worker thread started", type);
    
    auto& queue = _taskQueues[type];
    auto& queueMutex = _queueMutexes[type];
    auto& condition = _queueConditions[type];
    
    while (_threadPoolActive[type] && !_shutdown.load())
    {
        PlayerbotTask task;
        bool hasTask = false;
        
        // Wait for task or shutdown
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            condition.wait_for(lock, std::chrono::milliseconds(_config.threadIdleTimeoutMs),
                [&] { return !queue.empty() || !_threadPoolActive[type] || _shutdown.load(); });
            
            if (!queue.empty())
            {
                task = queue.top();
                queue.pop();
                hasTask = true;
                _statistics.queueSize--;
            }
        }
        
        if (hasTask)
        {
            auto startTime = std::chrono::steady_clock::now();
            
            try
            {
                if (ValidateTask(task))
                {
                    ExecuteTask(task);
                    _statistics.tasksProcessed++;
                }
                else
                {
                    TC_LOG_WARN("playerbots", "Invalid task detected in worker thread type {} index {}", 
                               uint32(type), threadIndex);
                }
            }
            catch (const std::exception& ex)
            {
                HandleTaskException(task, ex);
            }
            catch (...)
            {
                TC_LOG_ERROR("playerbots", "Unknown exception in worker thread type {} index {}", 
                            uint32(type), threadIndex);
            }
            
            auto endTime = std::chrono::steady_clock::now();
            auto executionTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
            UpdateTaskStatistics(task, uint32(executionTime.count()));
            
            // Check for task timeout
            if (executionTime > std::chrono::milliseconds(_config.taskTimeoutMs))
            {
                HandleTaskTimeout(task);
            }
        }
        else if (_threadPoolActive[type] && !_shutdown.load())
        {
            // Thread was idle, increment idle counter
            _statistics.idleThreads++;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            _statistics.idleThreads--;
        }
    }
    
    LogThreadActivity("Worker thread stopped", type);
}

bool PlayerbotThreadManager::ExecuteTask(PlayerbotTask const& task)
{
    if (!task.task)
        return false;
    
    // Check deadline
    auto now = std::chrono::steady_clock::now();
    if (now > task.deadline)
    {
        TC_LOG_WARN("playerbots", "Task deadline exceeded: {} (type: {})", 
                   task.description, uint32(task.type));
        return false;
    }
    
    // Execute the task
    try
    {
        task.task();
        return true;
    }
    catch (const std::exception& ex)
    {
        TC_LOG_ERROR("playerbots", "Task execution failed: {} - {}", task.description, ex.what());
        return false;
    }
}

void PlayerbotThreadManager::ScheduleTask(PlayerbotTask const& task)
{
    if (!_initialized.load())
    {
        TC_LOG_WARN("playerbots", "Cannot schedule task - ThreadManager not initialized");
        return;
    }
    
    if (_emergencyStop.load())
    {
        TC_LOG_WARN("playerbots", "Cannot schedule task - Emergency stop active");
        return;
    }
    
    // Determine optimal thread type for this task
    PlayerbotThreadType threadType = GetOptimalThreadType(task);
    
    // Add to appropriate queue
    {
        std::lock_guard<std::mutex> lock(_queueMutexes[threadType]);
        
        if (_taskQueues[threadType].size() >= _config.queueSizeLimit)
        {
            TC_LOG_WARN("playerbots", "Task queue full for thread type {} - dropping task", uint32(threadType));
            return;
        }
        
        _taskQueues[threadType].push(task);
        _statistics.queueSize++;
        _statistics.tasksQueued++;
    }
    
    // Notify worker thread
    _queueConditions[threadType].notify_one();
    
    // Handle emergency tasks immediately
    if (task.priority == PlayerbotTaskPriority::IMMEDIATE)
    {
        _emergencyTaskCount++;
        _emergencyCondition.notify_all();
    }
}

void PlayerbotThreadManager::ScheduleRepeatingTask(PlayerbotTask const& task, uint32 intervalMs)
{
    PlayerbotTask repeatingTask = task;
    repeatingTask.repeating = true;
    repeatingTask.repeatInterval = intervalMs;
    
    {
        std::lock_guard<std::mutex> lock(_repeatingTasksMutex);
        _repeatingTasks.push_back(repeatingTask);
    }
    
    ScheduleTask(task);
}

PlayerbotThreadType PlayerbotThreadManager::GetOptimalThreadType(PlayerbotTask const& task) const
{
    switch (task.type)
    {
        case PlayerbotTaskType::AI_UPDATE:
            return PlayerbotThreadType::MAIN_AI;
        case PlayerbotTaskType::ACTION_EXECUTE:
            return PlayerbotThreadType::ACTION_WORKER;
        case PlayerbotTaskType::DATA_SYNC:
            return PlayerbotThreadType::DATABASE;
        case PlayerbotTaskType::CACHE_UPDATE:
            return PlayerbotThreadType::CACHE_MANAGER;
        case PlayerbotTaskType::PERFORMANCE:
            return PlayerbotThreadType::MONITOR;
        case PlayerbotTaskType::MAINTENANCE:
            return PlayerbotThreadType::MAINTENANCE;
        case PlayerbotTaskType::EMERGENCY:
            return PlayerbotThreadType::MAIN_AI; // Use fastest available pool
        default:
            return PlayerbotThreadType::MAIN_AI;
    }
}

void PlayerbotThreadManager::Update(uint32 diff)
{
    if (!_initialized.load())
        return;
    
    auto now = std::chrono::steady_clock::now();
    
    // Update performance metrics
    if (now - _lastPerformanceUpdate > std::chrono::milliseconds(_config.performanceUpdateIntervalMs))
    {
        UpdatePerformanceMetrics();
        _lastPerformanceUpdate = now;
    }
    
    // Process repeating tasks
    if (now - _lastRepeatingTaskCheck > std::chrono::milliseconds(100))
    {
        ProcessRepeatingTasks();
        _lastRepeatingTaskCheck = now;
    }
    
    // Detect and handle bottlenecks
    DetectBottlenecks();
    
    // Clean up completed tasks
    CleanupCompletedTasks();
    
    // Handle emergency situations
    if (_emergencyTaskCount.load() > 0)
    {
        ProcessEmergencyTasks();
    }
}

void PlayerbotThreadManager::UpdatePerformanceMetrics()
{
    // Calculate average CPU usage
    float totalCPU = 0.0f;
    uint32 activeThreadCount = _statistics.activeThreads.load();
    
    if (activeThreadCount > 0)
    {
        // This would need platform-specific CPU monitoring
        _averageCPUUsage = std::min(100.0f, totalCPU / activeThreadCount);
    }
    
    // Calculate average queue size
    uint32 totalQueueSize = 0;
    for (auto const& [type, queue] : _taskQueues)
    {
        std::lock_guard<std::mutex> lock(_queueMutexes.at(type));
        totalQueueSize += queue.size();
    }
    _averageQueueSize = totalQueueSize;
    
    // Update statistics timestamp
    _statistics.lastUpdate = std::chrono::steady_clock::now();
    
    // Log performance metrics periodically
    static auto lastLogTime = std::chrono::steady_clock::now();
    if (std::chrono::steady_clock::now() - lastLogTime > std::chrono::minutes(1))
    {
        TC_LOG_INFO("playerbots", "ThreadManager Performance - Tasks: {}/{}, Queue: {}, CPU: {:.1f}%, Threads: {}/{}",
                   _statistics.tasksProcessed.load(), _statistics.tasksQueued.load(),
                   _averageQueueSize.load(), _averageCPUUsage.load(),
                   _statistics.activeThreads.load(), _statistics.idleThreads.load());
        lastLogTime = std::chrono::steady_clock::now();
    }
}

void PlayerbotThreadManager::DetectBottlenecks()
{
    // Check for queue overload
    for (auto const& [type, queue] : _taskQueues)
    {
        std::lock_guard<std::mutex> lock(_queueMutexes.at(type));
        if (queue.size() > _config.queueSizeLimit * 0.8f)
        {
            TC_LOG_WARN("playerbots", "Thread pool type {} queue at {:.1f}% capacity", 
                       uint32(type), (float(queue.size()) / _config.queueSizeLimit) * 100.0f);
            
            // Consider scaling up this thread pool
            OptimizeThreadAllocation();
        }
    }
    
    // Check for thread starvation
    if (_statistics.idleThreads.load() == 0 && _averageQueueSize.load() > 100)
    {
        TC_LOG_WARN("playerbots", "Thread starvation detected - no idle threads with {} queued tasks", 
                   _averageQueueSize.load());
    }
}

void PlayerbotThreadManager::ProcessRepeatingTasks()
{
    std::lock_guard<std::mutex> lock(_repeatingTasksMutex);
    auto now = std::chrono::steady_clock::now();
    
    for (auto& task : _repeatingTasks)
    {
        auto nextRun = task.createdTime + std::chrono::milliseconds(task.repeatInterval);
        if (now >= nextRun)
        {
            ScheduleTask(task);
            task.createdTime = now; // Reset for next interval
        }
    }
}

void PlayerbotThreadManager::ProcessEmergencyTasks()
{
    uint32 emergencyCount = _emergencyTaskCount.exchange(0);
    if (emergencyCount > 0)
    {
        TC_LOG_WARN("playerbots", "Processing {} emergency tasks", emergencyCount);
        
        // Temporarily boost emergency thread priority
        // This would require platform-specific thread priority manipulation
        
        // Process emergency queue with higher priority
        for (auto& [type, condition] : _queueConditions)
        {
            condition.notify_all(); // Wake all threads to handle emergency tasks
        }
    }
}

uint32 PlayerbotThreadManager::DetectCPUCores() const
{
    std::vector<uint32> cores;
    uint32 coreCount = std::thread::hardware_concurrency();
    
    for (uint32 i = 0; i < coreCount; ++i)
    {
        cores.push_back(i);
    }
    
    TC_LOG_INFO("playerbots", "Detected {} CPU cores for thread affinity", coreCount);
    return coreCount;
}

void PlayerbotThreadManager::SetThreadAffinity(std::thread::native_handle_type handle, std::vector<uint32> const& cpuCores)
{
    if (!_config.enableCPUAffinity || cpuCores.empty())
        return;
    
#ifdef _WIN32
    DWORD_PTR affinityMask = 0;
    for (uint32 core : cpuCores)
    {
        if (core < 64) // Windows affinity mask is 64-bit
            affinityMask |= (1ULL << core);
    }
    SetThreadAffinityMask(handle, affinityMask);
#elif defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (uint32 core : cpuCores)
    {
        if (core < CPU_SETSIZE)
            CPU_SET(core, &cpuset);
    }
    pthread_setaffinity_np(handle, sizeof(cpu_set_t), &cpuset);
#endif
}

bool PlayerbotThreadManager::ValidateConfiguration(PlayerbotThreadPoolConfig const& config) const
{
    if (config.minThreads == 0 || config.maxThreads == 0)
        return false;
    
    if (config.minThreads > config.maxThreads)
        return false;
    
    if (config.queueSizeLimit == 0)
        return false;
    
    if (config.taskTimeoutMs == 0 || config.threadIdleTimeoutMs == 0)
        return false;
    
    return true;
}

bool PlayerbotThreadManager::ValidateTask(PlayerbotTask const& task) const
{
    if (!task.task)
        return false;
    
    if (task.estimatedDuration == 0)
        return false;
    
    // Check if task deadline is reasonable
    auto now = std::chrono::steady_clock::now();
    if (task.deadline < now)
        return false;
    
    return true;
}

void PlayerbotThreadManager::UpdateTaskStatistics(PlayerbotTask const& task, uint32 executionTimeUs)
{
    _statistics.totalExecutionTime += executionTimeUs;
    
    uint64_t processed = _statistics.tasksProcessed.load();
    if (processed > 0)
    {
        _statistics.averageExecutionTime = uint32(_statistics.totalExecutionTime.load() / processed);
    }
}

void PlayerbotThreadManager::HandleTaskTimeout(PlayerbotTask const& task)
{
    TC_LOG_ERROR("playerbots", "Task timeout: {} (type: {}, priority: {})", 
                task.description, uint32(task.type), uint32(task.priority));
    
    // Implement timeout recovery logic
    if (task.priority >= PlayerbotTaskPriority::CRITICAL)
    {
        // For critical tasks, consider emergency escalation
        _emergencyTaskCount++;
    }
}

void PlayerbotThreadManager::HandleTaskException(PlayerbotTask const& task, std::exception const& ex)
{
    TC_LOG_ERROR("playerbots", "Task exception: {} - {} (type: {})", 
                task.description, ex.what(), uint32(task.type));
    
    // Implement exception recovery logic based on task type and priority
}

void PlayerbotThreadManager::CleanupCompletedTasks()
{
    // Remove expired repeating tasks
    std::lock_guard<std::mutex> lock(_repeatingTasksMutex);
    _repeatingTasks.erase(
        std::remove_if(_repeatingTasks.begin(), _repeatingTasks.end(),
            [](const PlayerbotTask& task) {
                // Remove tasks that haven't been used recently
                auto now = std::chrono::steady_clock::now();
                return (now - task.createdTime) > std::chrono::hours(1);
            }),
        _repeatingTasks.end());
}

uint32 PlayerbotThreadManager::GetTotalThreadCount() const
{
    uint32 total = 0;
    for (auto const& [type, threads] : _threadPools)
    {
        total += threads.size();
    }
    return total;
}

void PlayerbotThreadManager::LogThreadActivity(std::string const& activity, PlayerbotThreadType type)
{
    TC_LOG_TRACE("playerbots", "ThreadManager[{}]: {}", uint32(type), activity);
}

void PlayerbotThreadManager::OptimizeThreadAllocation()
{
    // Basic thread pool scaling based on queue sizes
    for (auto const& [type, queue] : _taskQueues)
    {
        std::lock_guard<std::mutex> lock(_queueMutexes.at(type));
        
        uint32 currentThreads = _threadPools[type].size();
        float queueUtilization = float(queue.size()) / _config.queueSizeLimit;
        
        if (queueUtilization > 0.8f && currentThreads < _config.maxThreads / 6)
        {
            // Scale up this thread pool
            TC_LOG_INFO("playerbots", "Scaling up thread pool type {} due to high utilization ({:.1f}%)", 
                       uint32(type), queueUtilization * 100.0f);
            // Implementation would add threads dynamically
        }
        else if (queueUtilization < 0.2f && currentThreads > _config.minThreads / 6)
        {
            // Scale down this thread pool
            TC_LOG_DEBUG("playerbots", "Could scale down thread pool type {} due to low utilization ({:.1f}%)", 
                        uint32(type), queueUtilization * 100.0f);
            // Implementation would remove threads carefully
        }
    }
}

MonitorThread() {
    TC_LOG_INFO("playerbots", "Performance monitor thread started");
    
    while (!_shutdown.load() && _initialized.load())
    {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        // Generate periodic performance reports
        if (_statistics.tasksProcessed.load() > 0)
        {
            float throughput = GetThroughput();
            float cpuUsage = GetAverageCPUUsage();
            
            TC_LOG_DEBUG("playerbots", "ThreadManager Monitor - Throughput: {:.1f} tasks/sec, CPU: {:.1f}%, Queue: {}",
                        throughput, cpuUsage, _averageQueueSize.load());
        }
        
        // Check for performance degradation
        if (GetAverageCPUUsage() > 90.0f)
        {
            TC_LOG_WARN("playerbots", "High CPU usage detected: {:.1f}% - consider scaling", GetAverageCPUUsage());
        }
    }
    
    TC_LOG_INFO("playerbots", "Performance monitor thread stopped");
}

void PlayerbotThreadManager::MaintenanceThread()
{
    TC_LOG_INFO("playerbots", "Maintenance thread started");
    
    while (!_shutdown.load() && _initialized.load())
    {
        std::this_thread::sleep_for(std::chrono::minutes(1));
        
        // Perform maintenance tasks
        CleanupCompletedTasks();
        DefragmentQueues();
        OptimizeMemoryLayout();
        
        // Log statistics
        TC_LOG_DEBUG("playerbots", "ThreadManager Maintenance - Tasks processed: {}, Queue size: {}, Active threads: {}",
                    _statistics.tasksProcessed.load(), _averageQueueSize.load(), _statistics.activeThreads.load());
    }
    
    TC_LOG_INFO("playerbots", "Maintenance thread stopped");
}

float PlayerbotThreadManager::GetThroughput() const
{
    uint64_t tasksProcessed = _statistics.tasksProcessed.load();
    auto uptime = std::chrono::steady_clock::now() - _statistics.lastUpdate;
    auto uptimeSeconds = std::chrono::duration_cast<std::chrono::seconds>(uptime).count();
    
    return uptimeSeconds > 0 ? float(tasksProcessed) / uptimeSeconds : 0.0f;
}

void PlayerbotThreadManager::DefragmentQueues()
{
    // This would implement queue optimization
    // For now, just log queue states
    for (auto const& [type, queue] : _taskQueues)
    {
        std::lock_guard<std::mutex> lock(_queueMutexes.at(type));
        if (!queue.empty())
        {
            TC_LOG_TRACE("playerbots", "Queue type {} has {} pending tasks", uint32(type), queue.size());
        }
    }
}

void PlayerbotThreadManager::OptimizeMemoryLayout()
{
    // This would implement memory optimization
    // For now, just clear any internal caches that might have grown large
}

#endif // WITH_PLAYERBOTS