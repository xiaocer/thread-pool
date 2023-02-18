#pragma once
#include "TaskQueue.h"
#include "TaskQueue.cpp"

template<typename T>
class ThreadPool {
private:
    TaskQueue<T>* taskQueue;  // 任务队列
    pthread_t managerId;   // 管理者线程id
    pthread_t* threadsIds; // 工作的线程
    int minNum;            // 最小线程数量
    int maxNum;            // 最大线程数量
    int busyNum;           // 忙的线程的个数
    int liveNum;           // 存活的线程的个数
    int exitNum;           // 待销毁的线程的个数
    pthread_mutex_t mutexPool; // 锁整个线程池
    pthread_cond_t notEmpty;   // 任务队列是否为空，阻塞消费者线程的。任务队列可以无限生产，这里不需要阻塞生产者的条件变量

    bool shutdown;             // 是否销毁线程池

    static const int NUMBER = 2;   // 每次添加线程、销毁线程的个数
    // 工作的线程的执行函数
    static void* worker(void* arg);
    // 管理线程执行的函数
    static void* manager(void* arg);
    // 单个线程退出
    void threadExit();
public:
    //   创建线程池并初始化
    ThreadPool(int min, int max);
    // 销毁线程池
    ~ThreadPool();
    // 给线程池添加任务
    void addTask(Task<T> task);
    // 获取线程池中忙的线程个数
    int getBusyNum(); 
    // 获取线程池中活的线程的个数
    int getLiveNum();

};