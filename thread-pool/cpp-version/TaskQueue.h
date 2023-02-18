#pragma once
#include <queue>
#include <pthread.h>

using callback = void (*)(void*);
// 任务结构体
template <typename T>
struct Task {
public:
    Task() {
        function = nullptr;
        arg = nullptr;
    }
    Task(callback function, void* arg) : function(function), arg((T*)arg) {}
    callback getFunction()const {
        return this->function;
    }
    T* getArg()const {return this->arg;}
private:
    callback function;
    T* arg;
};

// 任务队列
template <typename T>
class TaskQueue {
private:
    std::queue<Task<T>> m_taskQ;
    pthread_mutex_t m_mutex; 
public:
    TaskQueue();
    ~TaskQueue();

    // 添加任务
    void addTask(Task<T> task);
    void addTask(callback function, void* arg);
    // 取出一个任务
    Task<T> getTask();
    // 获取当前任务的个数
    inline int size() {
        return m_taskQ.size();
    }
};