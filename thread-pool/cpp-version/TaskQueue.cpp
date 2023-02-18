#include "TaskQueue.h"

template <typename T>
TaskQueue<T>::TaskQueue() {
    pthread_mutex_init(&m_mutex, NULL);
}
template <typename T>
TaskQueue<T>::~TaskQueue() {
    pthread_mutex_destroy(&m_mutex);
}

// 添加任务
template <typename T>
void TaskQueue<T>::addTask(Task<T> task) {
    pthread_mutex_lock(&m_mutex);
    this->m_taskQ.push(task);
    pthread_mutex_unlock(&m_mutex);
}
template <typename T>
void TaskQueue<T>::addTask(callback function, void* arg) {
    pthread_mutex_lock(&m_mutex);
    this->m_taskQ.push(Task(function, arg));
    pthread_mutex_unlock(&m_mutex);
}

// 取出一个任务
template <typename T>
Task<T> TaskQueue<T>::getTask() {
    Task<T> task;
    pthread_mutex_lock(&m_mutex);
    if (!m_taskQ.empty()) {
        task = this->m_taskQ.front();
        this->m_taskQ.pop();
    }
    pthread_mutex_unlock(&m_mutex);
    return task;
}