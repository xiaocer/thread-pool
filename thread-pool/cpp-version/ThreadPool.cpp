#include "ThreadPool.h"
#include <iostream>
#include <cstring>
#include <string>
#include <unistd.h>

// 工作的线程的执行函数
template <typename T>
void* ThreadPool<T>::worker(void* arg) {
    ThreadPool* pool = static_cast<ThreadPool*>(arg);
    while (true) {
        pthread_mutex_lock(&pool->mutexPool);
        // 当前队列是否为空
        while (!pool->shutdown && pool->taskQueue->size() == 0  ) {
            // 阻塞工作线程
            pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);
            // 判断是否需要销毁线程
            if (pool->exitNum > 0) {
                pool->exitNum--;
                if (pool->liveNum > pool->minNum) {
                    pool->liveNum--;
                    pthread_mutex_unlock(&pool->mutexPool);
                    pool->threadExit();
                } 
            }
        }
        // 判断线程池是否被关闭
        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->mutexPool);
            pool->threadExit();
        }
        // 从任务队列中取出任务进行消费
        Task<T> task = pool->taskQueue->getTask();
        pool->busyNum++;
        std::cout << "thread:" << std::to_string(pthread_self()) << "start working..." << std::endl;
        task.getFunction()(task.getArg());
        if (task.getArg()) delete task.getArg(); 
        // 解锁
        pthread_mutex_unlock(&pool->mutexPool);
        std::cout << "thread:" << std::to_string(pthread_self()) << "end working..." << std::endl;

        pthread_mutex_lock(&pool->mutexPool);
        pool->busyNum--;  
        pthread_mutex_unlock(&pool->mutexPool);
    }
}
// 管理线程执行的函数
template <typename T>
void* ThreadPool<T>::manager(void* arg){
    ThreadPool* pool = static_cast<ThreadPool*>(arg);
    while (!pool->shutdown) {
        sleep(3); // 每隔三秒钟检测一次

        // 取出线程池中的任务数量和当前存活线程的数量、忙的线程个数
        pthread_mutex_lock(&pool->mutexPool);
        int taskNum = pool->taskQueue->size();
        int liveNum = pool->liveNum;
        int busyNum = pool->busyNum;
        pthread_mutex_unlock(&pool->mutexPool);

        // 添加线程：任务的个数>存活的线程个数 && 存活的线程个数 < 最大的线程个数
        if (taskNum > liveNum && liveNum < pool->maxNum) {
            pthread_mutex_lock(&pool->mutexPool);
            int counter = 0;
            for (int i = 0; i < pool->maxNum && counter < pool->NUMBER
                && pool->liveNum < pool->maxNum; ++i) {
                    if (pool->threadsIds[i] == 0) {
                        pthread_create(&pool->threadsIds[i], NULL, worker, pool);
                        counter++;
                        pool->liveNum++;
                    }
                }
            pthread_mutex_unlock(&pool->mutexPool);
        }

        // 销毁线程:存活的线程数 > 最小线程数量 && 忙的线程个数 * 2 < 存活的线程数
        if (busyNum * 2 < liveNum && liveNum > pool->minNum) {
            pthread_mutex_lock(&pool->mutexPool);
            pool->exitNum = pool->NUMBER;
            pthread_mutex_unlock(&pool->mutexPool);
            // 让工作线程自杀
            for (int i = 0; i < NUMBER; i++) {
                // 唤醒阻塞的生产者线程
                pthread_cond_signal(&pool->notEmpty);
            }
        }
    }
    return nullptr;
}
// 单个线程退出
template <typename T>
void ThreadPool<T>::threadExit(){
    pthread_t tid = pthread_self();
    for (int i = 0; i < this->maxNum; i++) {
        if (threadsIds[i] == tid) {
            threadsIds[i] = 0;
            std::cout << "threadExit() called," << std::to_string(tid) << "exiting..." << std::endl;
            break;
        }
    }
    pthread_exit(NULL);
}
//   创建线程池并初始化
template <typename T>
ThreadPool<T>::ThreadPool(int min, int max){
    do {
         // 实例化任务队列
        taskQueue = new(std::nothrow)TaskQueue<T>(); // nothrow表示分配内存失败不抛出异常，而是返回空指针                 
        if (taskQueue == nullptr) {
            std::cout << "new taskQueue error" << std::endl;
            break;
        }

        threadsIds = new (std::nothrow)pthread_t[max];
        if (threadsIds == nullptr) {
            std::cout << "new threadsIds error" << std::endl;
            break;
        }
        memset(threadsIds, 0, sizeof(pthread_t) * max);

        this->maxNum = max;
        this->minNum = min; // 创建了min个工作线程
        this->busyNum = 0;
        this->exitNum = 0;
        this->liveNum = min;

        if (pthread_mutex_init(&this->mutexPool, NULL) != 0 ||
            pthread_cond_init(&this->notEmpty, NULL) != 0) {
                std::cout << "mutex or condition init fail" << std::endl;
                break;
        }

        this->shutdown = false;

        // 创建线程
        pthread_create(&managerId, NULL, manager, this);
        for (int i = 0; i < min; ++i) {
            pthread_create(&threadsIds[i], NULL, worker, this);
        }

    } while(0);
    if (threadsIds) {
        delete[] threadsIds;
    }
    if (taskQueue) {
        delete taskQueue;
    }
}
// 销毁线程池
template <typename T>
ThreadPool<T>::~ThreadPool(){
    // 关闭线程池
    this->shutdown = true;
    // 阻塞回收管理者线程
    pthread_join(this->managerId, NULL);
    // 唤醒阻塞的消费者线程
    for (int i = 0; i < this->liveNum; ++i) {
        pthread_cond_signal(&this->notEmpty);
    }

    if (this->taskQueue) delete taskQueue;
    if (this->threadsIds) delete[] threadsIds;

    pthread_mutex_destroy(&this->mutexPool);
    pthread_cond_destroy(&this->notEmpty);
}
// 给线程池添加任务
template <typename T>
void ThreadPool<T>::addTask(Task<T> task){
    if (this->shutdown) {
        return;
    }
    this->taskQueue->addTask(task);
    // 通知消费者
    pthread_cond_signal(&notEmpty);
}
// 获取线程池中忙的线程个数
template <typename T>
int ThreadPool<T>::getBusyNum(){
    pthread_mutex_lock(&this->mutexPool);
    int busyNum = this->busyNum;
    pthread_mutex_unlock(&this->mutexPool);
    return busyNum;
}
// 获取线程池中活的线程的个数
template <typename T>
int ThreadPool<T>::getLiveNum(){
    pthread_mutex_lock(&this->mutexPool);
    int liveNum = this->liveNum;
    pthread_mutex_unlock(&this->mutexPool);
    return liveNum;
}