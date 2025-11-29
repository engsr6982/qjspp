#include "qjspp/runtime/TaskQueue.hpp"

namespace qjspp {


TaskQueue::Task::Task(TaskCallback cb, void* d, std::chrono::steady_clock::time_point dt)
: callback_(cb),
  data_(d),
  dueTime_(dt) {}

bool TaskQueue::Task::operator>(const Task& other) const { return dueTime_ > other.dueTime_; }

TaskQueue::TaskQueue() : shutdown_(false) {}
TaskQueue::~TaskQueue() {
    shutdown(true);
    loopAndWait(); // 等待所有任务完成
}


void TaskQueue::postTask(TaskCallback callback, void* data, int delayMs) {
    auto dueTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs);

    std::lock_guard<std::mutex> lock(mutex_);
    tasks_.push({callback, data, dueTime});
    cv_.notify_one();
}

bool TaskQueue::loopOnce() {
    std::vector<Task> dueTasks;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto                        now = std::chrono::steady_clock::now();

        while (!tasks_.empty() && tasks_.top().dueTime_ <= now) {
            dueTasks.push_back(tasks_.top());
            tasks_.pop();
        }
    }

    // 执行到期的任务
    for (const auto& task : dueTasks) {
        task.callback_(task.data_);
    }

    return !dueTasks.empty(); // 如果有任务执行，返回true
}

void TaskQueue::loopAndWait() {
    while (true) {
        if (shutdown_) {
            if (awaitTasks_ && !tasks_.empty()) {
                while (loopOnce()) {} // 执行所有任务
            }
            break;
        }
        if (!loopOnce()) {
            // 没有任务时等待
            std::unique_lock<std::mutex> lock(mutex_);
            if (tasks_.empty() && !shutdown_) {
                cv_.wait_for(lock, std::chrono::milliseconds(100));
            }
        }
    }
}

void TaskQueue::shutdown(bool wait) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_   = true;
        awaitTasks_ = wait;
    }
    cv_.notify_all();
}


} // namespace qjspp