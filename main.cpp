#include "modern_thread_pool.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

// 获取当前时间的辅助函数
std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_c), "%H:%M:%S");
    return ss.str();
}

// 模拟耗时操作
void simulateWork(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

int main() {
    // 获取线程池实例，设置2个通用线程
    auto& pool = ModernThreadPool::getInstance(2);
    
    // 添加专用线程
    pool.addDedicatedThread("logger");    // 日志处理线程
    pool.addDedicatedThread("io");        // IO处理线程
    pool.addDedicatedThread("network");   // 网络处理线程

    std::cout << "线程池初始化完成：\n"
              << "通用线程数: " << pool.getGeneralThreadCount() << "\n"
              << "专用线程数: " << pool.getDedicatedThreadCount() << "\n\n";

    // 提交高优先级的日志任务
    auto logTask = pool.submit("logger", 10, [](const std::string& msg) {
        std::cout << getCurrentTime() << " [Logger] " << msg << std::endl;
        simulateWork(500);  // 模拟日志写入耗时
        return true;
    }, "高优先级日志");

    // 提交普通优先级的IO任务
    auto ioTask = pool.submit("io", 5, []() {
        std::cout << getCurrentTime() << " [IO] 开始文件操作\n";
        simulateWork(1000);  // 模拟IO操作耗时
        return "文件操作完成";
    });

    // 提交低优先级的网络任务
    auto networkTask = pool.submit("network", 1, []() {
        std::cout << getCurrentTime() << " [Network] 开始网络请求\n";
        simulateWork(1500);  // 模拟网络请求耗时
        return 200;  // 模拟HTTP状态码
    });

    // 提交多个不同优先级的通用任务
    std::vector<std::future<int>> generalTasks;
    for (int i = 0; i < 5; ++i) {
        generalTasks.push_back(pool.submit("general", i, [i]() {
            std::cout << getCurrentTime() << " [General] 任务 " << i 
                      << " (优先级:" << i << ") 开始执行\n";
            simulateWork(800);  // 模拟通用任务处理
            return i * 10;
        }));
    }

    // 等待并获取结果
    try {
        // 等待日志任务完成
        if (logTask.get()) {
            std::cout << getCurrentTime() << " 日志任务完成\n";
        }

        // 等待IO任务完成
        std::cout << getCurrentTime() << " IO结果: " << ioTask.get() << "\n";

        // 等待网络任务完成
        int statusCode = networkTask.get();
        std::cout << getCurrentTime() << " 网络请求状态码: " << statusCode << "\n";

        // 等待所有通用任务完成
        for (size_t i = 0; i < generalTasks.size(); ++i) {
            int result = generalTasks[i].get();
            std::cout << getCurrentTime() << " 通用任务 " << i << " 结果: " << result << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "发生错误: " << e.what() << std::endl;
    }

    std::cout << "\n所有任务执行完成！" << std::endl;
    return 0;
} 