#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <unordered_map>
#include <memory>
#include <string>

class ModernThreadPool {
private:
    // 自定义删除器
    struct Deleter {
        void operator()(ModernThreadPool* ptr) const {
            delete ptr;
        }
    };

public:
    using TaskType = std::string;  // 用户可以用字符串来标识任务类型

    // 获取单例实例
    static ModernThreadPool& getInstance(size_t generalThreads = 2) {
        static std::once_flag flag;
        std::call_once(flag, [](size_t threads) {
            instance_.reset(new ModernThreadPool(threads));
        }, generalThreads);
        return *instance_;
    }

    // 禁用拷贝构造和赋值操作
    ModernThreadPool(const ModernThreadPool&) = delete;
    ModernThreadPool& operator=(const ModernThreadPool&) = delete;
    // 禁用移动构造和赋值操作
    ModernThreadPool(ModernThreadPool&&) = delete;
    ModernThreadPool& operator=(ModernThreadPool&&) = delete;

    // 添加专用线程
    void addDedicatedThread(const TaskType& type);

    // 提交任务的模板方法
    template<class F, class... Args>
    auto submit(const TaskType& type, F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result_t<F, Args...>>;

    // 获取通用线程数量
    size_t getGeneralThreadCount() const { return generalWorkers.size(); }

    // 获取专用线程数量
    size_t getDedicatedThreadCount() const { return dedicatedWorkers.size(); }

private:
    friend struct Deleter;  // 允许删除器访问私有析构函数

    // 私有构造函数
    explicit ModernThreadPool(size_t generalThreads);
    // 私有析构函数
    ~ModernThreadPool();

    // 工作线程函数
    void workerThread(const TaskType& type);

    // 单例实例指针，使用自定义删除器
    static inline std::unique_ptr<ModernThreadPool, Deleter> instance_;

    static constexpr const char* GENERAL_TASK = "general";  // 通用任务的标识符
    std::vector<std::thread> generalWorkers;
    std::unordered_map<TaskType, std::thread> dedicatedWorkers;
    
    // 每种类型的任务队列
    std::unordered_map<TaskType, std::queue<std::function<void()>>> taskQueues;
    
    // 同步原语
    mutable std::unordered_map<TaskType, std::mutex> queueMutexes;
    std::unordered_map<TaskType, std::condition_variable> conditions;
    
    std::atomic<bool> stop{false};
    mutable std::mutex stopMutex;
};

// 模板方法的实现必须在头文件中
template<class F, class... Args>
auto ModernThreadPool::submit(const TaskType& type, F&& f, Args&&... args) 
    -> std::future<typename std::invoke_result_t<F, Args...>> 
{
    using return_type = typename std::invoke_result_t<F, Args...>;
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> res = task->get_future();
    {
        std::lock_guard<std::mutex> lock(queueMutexes[type]);
        taskQueues[type].emplace([task](){ (*task)(); });
    }
    
    conditions[type].notify_one();
    return res;
} 