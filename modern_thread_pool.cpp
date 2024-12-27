#include "modern_thread_pool.hpp"

ModernThreadPool::ModernThreadPool(size_t generalThreads) {
    // 初始化通用工作线程
    for (size_t i = 0; i < generalThreads; ++i) {
        generalWorkers.emplace_back(&ModernThreadPool::workerThread, this, GENERAL_TASK);
    }
}

ModernThreadPool::~ModernThreadPool() {
    {
        std::lock_guard<std::mutex> lock(stopMutex);
        stop = true;
    }
    
    // 通知所有条件变量
    for (auto& [type, condition] : conditions) {
        condition.notify_all();
    }
    
    // 等待所有线程完成
    for (auto& worker : generalWorkers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    for (auto& [type, worker] : dedicatedWorkers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ModernThreadPool::addDedicatedThread(const TaskType& type) {
    // 确保不重复添加专用线程
    if (dedicatedWorkers.find(type) == dedicatedWorkers.end()) {
        dedicatedWorkers.emplace(type, 
            std::thread(&ModernThreadPool::workerThread, this, type));
    }
}

void ModernThreadPool::workerThread(const TaskType& type) {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queueMutexes[type]);
            conditions[type].wait(lock, [this, &type]() {
                return stop || !taskQueues[type].empty();
            });
            
            if (stop && taskQueues[type].empty()) {
                return;
            }
            
            // 获取优先级最高的任务
            task = std::move(taskQueues[type].top().func);
            taskQueues[type].pop();
        }
        task();
    }
} 