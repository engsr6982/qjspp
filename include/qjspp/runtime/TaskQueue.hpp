#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>


namespace qjspp {

class TaskQueue {
public:
    using TaskCallback = void (*)(void* data);

    struct Task {
        TaskCallback                          callback_; // 回调函数
        void*                                 data_;     // 数据指针
        std::chrono::steady_clock::time_point dueTime_;  // 执行时间

        Task(TaskCallback cb, void* d, std::chrono::steady_clock::time_point dt);

        bool operator>(const Task& other) const;
    };

    TaskQueue();
    ~TaskQueue();

    // 发布任务
    void postTask(TaskCallback callback, void* data = nullptr, int delayMs = 0);

    // 单次循环
    bool loopOnce();

    // 持续循环直到关闭
    void loopAndWait();

    // 关闭队列
    void shutdown(bool wait = false);

private:
    std::priority_queue<Task, std::vector<Task>, std::greater<>> tasks_;
    std::mutex                                                   mutex_;
    std::condition_variable                                      cv_;
    std::atomic<bool>                                            shutdown_;   // 关闭队列
    std::atomic<bool>                                            awaitTasks_; // 等待任务完成
};


} // namespace qjspp