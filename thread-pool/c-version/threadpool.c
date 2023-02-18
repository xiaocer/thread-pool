#include "threadpool.h"
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
// 任务的定义
typedef struct Task {
	void* (*function)(void* arg);
	void* arg;
}Task;

// 线程池定义
struct ThreadPool {
	Task* taskQueue;			// 任务队列
	int queueCapacity;			// 队列容量
	int queueSize;				// 当前任务队列的任务个数
	int queueFront;				// 队头取数据
	int queueRear;				// 队尾放数据
	
	pthread_t managetId;		// 管理者线程id
	pthread_t* workerThreadsId;	// 工作的线程id
	int minNum;					// 最小线程数量
	int maxNum;					// 最大线程数量
	int busyNum;				// 忙的线程数量
	int liveNum;				// 存活的线程数量
	int exitNum;				// 待销毁的线程数量
	pthread_mutex_t mutexPool;	// 锁定整个线程池的互斥锁
	pthread_mutex_t mutexBusy;	// 锁定busyNum的互斥锁，使用细粒度的互斥锁提高效率
	pthread_cond_t notFull;     // 任务队列是不是满了
	pthread_cond_t notEmpty;    // 任务队列是不是空了

	int shutDown;				// 是否销毁线程池，0表示不销毁，1表示销毁
};

const int NUMBER = 2; // 每次添加/销毁的线程数量

ThreadPool* threadPoolCreate(int min, int max, int queueSize)
{
	ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
	do {
		if (pool == NULL) {
			printf("malloc ThreadPool fail\n");
			break;
		}

		// 工作队列的初始化
		pool->taskQueue = (Task*)malloc(sizeof(Task) * queueSize);
		if (pool->taskQueue == NULL) {
			printf("malloc taskQueue fail\n");
			break;
		}
		pool->queueCapacity = queueSize;
		pool->queueSize = 0;
		pool->queueFront = 0;
		pool->queueRear = 0;

		// 线程的初始化
		pool->workerThreadsId = (pthread_t*)malloc(sizeof(pthread_t) * max);
		if (pool->workerThreadsId == NULL) {
			printf("malloc workerThreadsId fail\n");
			break;
		}
		// 将每个工作线程初始化为0
		memset(pool->workerThreadsId, 0, sizeof(pthread_t) * max);
		pthread_create(&pool->managetId, NULL, manager, pool);
		for (int i = 0; i < min; i++) {
			pthread_create(&pool->workerThreadsId[i], NULL, worker, pool);
		}
		pool->minNum = min;
		pool->maxNum = max;
		pool->busyNum = 0; // 由于在线程池刚初始化，尚未向工作队列添加任务
		pool->liveNum = min;
		pool->exitNum = 0;

		// 条件变量和互斥锁的初始化
		if (pthread_mutex_init(&pool->mutexPool, NULL) != 0 ||
			pthread_mutex_init(&pool->mutexBusy, NULL) != 0 ||
			pthread_cond_init(&pool->notEmpty, NULL) != 0 ||
			pthread_cond_init(&pool->notFull, NULL) != 0) {
			printf("cond/mutex init fail\n");
			break;
		}

		pool->shutDown = 0;
		printf("threadPoolCreate() success\n");
		return pool;
	} while (0);

	// 防止资源泄漏
	if (pool && pool->taskQueue) free(pool->taskQueue);
	if (pool && pool->workerThreadsId) free(pool->workerThreadsId);
	if (pool) free(pool);

	return NULL;
}

int threadPoolDestroy(ThreadPool* pool)
{
	if (pool == NULL)
		return -1;
	// 关闭线程池
	pool->shutDown = 1;
	// 阻塞回收管理者线程
	pthread_join(pool->managetId, NULL);
	// 唤醒阻塞的消费者线程
	for (int i = 0; i < pool->liveNum; i++) {
		pthread_cond_signal(&pool->notEmpty);
	}
	// 释放堆内存
	if (pool && pool->taskQueue) free(pool->taskQueue);
	if (pool && pool->workerThreadsId) free(pool->workerThreadsId);
	pthread_mutex_destroy(&pool->mutexBusy);
	pthread_mutex_destroy(&pool->mutexPool);
	pthread_cond_destroy(&pool->notEmpty);
	pthread_cond_destroy(&pool->notFull);
	
	free(pool);
	pool = NULL;

	return 0;
}

void threadPoolAdd(ThreadPool* pool, void*(*func)(void*), void* arg)
{
	pthread_mutex_lock(&pool->mutexPool);
	while (pool->queueSize == pool->queueCapacity && !pool->shutDown) {
		// 阻塞生产者线程
		pthread_cond_wait(&pool->notFull, &pool->mutexPool);
	}
	if (pool->shutDown) {
		pthread_mutex_unlock(&pool->mutexPool);
		return;
	}
	// 添加任务
	pool->taskQueue[pool->queueRear].arg = arg;
	pool->taskQueue[pool->queueRear].function = func;
	pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;
	pool->queueSize++;

	// 通知消费者线程消费
	pthread_cond_signal(&pool->notEmpty);
	pthread_mutex_unlock(&pool->mutexPool);
}

int threadPoolBusyNum(ThreadPool* pool)
{
	int busyNum = -1;
	pthread_mutex_lock(&pool->mutexBusy);
	if (pool != NULL) {
		busyNum = pool->busyNum;
	}
	pthread_mutex_unlock(&pool->mutexBusy);
	return busyNum;
}

int threadPoolAliveNum(ThreadPool* pool)
{
	int liveNum = -1;
	pthread_mutex_lock(&pool->mutexPool);
	if (pool != NULL) {
		liveNum = pool->liveNum;
	}
	pthread_mutex_unlock(&pool->mutexPool);
	return liveNum;
}

void* worker(void* arg)
{
	ThreadPool* pool = (ThreadPool*)arg;
	while (1) {
		pthread_mutex_lock(&pool->mutexPool);
		while (pool->queueSize == 0 && !pool->shutDown) {
			// 阻塞工作者线程
			pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);
			if (pool->exitNum > 0) {
				pool->exitNum--;
				if (pool->liveNum > pool->minNum) {
					pool->liveNum--;
					pthread_mutex_unlock(&pool->mutexPool);
					threadExit(pool);
				}
			}
		}
		if (pool->shutDown) {
			pthread_mutex_unlock(&pool->mutexPool);
			threadExit(pool);
		}
		// 从任务队列中取出一个任务
		Task task;
		task.function = pool->taskQueue[pool->queueFront].function;
		task.arg = pool->taskQueue[pool->queueFront].arg;
		// 移动头节点，维护一个环形队列
		pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;
		pool->queueSize--;
		// 解锁
		pthread_mutex_unlock(&pool->mutexPool);
		pthread_cond_signal(&pool->notFull);

		printf("thread %ld start working...\n", pthread_self());
		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum++;
		pthread_mutex_unlock(&pool->mutexBusy);
		task.function(task.arg);
		if (task.arg)
			free(task.arg);
		printf("thread %ld end working...\n", pthread_self());

		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum--;
		pthread_mutex_unlock(&pool->mutexBusy);
	}
	
	return NULL;
}

void* manager(void* arg)
{
	ThreadPool* pool = (ThreadPool*)arg;
	while (!pool->shutDown) {
		// 每隔3秒钟检测一次
		sleep(3);
		// 取出任务队列中的任务数量和当前存活线程的数量
		pthread_mutex_lock(&pool->mutexPool);
		int queSize = pool->queueSize;
		int liveNum = pool->liveNum;
		pthread_mutex_unlock(&pool->mutexPool);

		// 取出忙的线程的数量
		pthread_mutex_lock(&pool->mutexBusy);
		int busyNum = pool->busyNum;
		pthread_mutex_unlock(&pool->mutexBusy);

		// 添加线程：当前存活的线程 < 线程池的最大线程数 && 任务数量 > 当前存活的线程
		if (queSize > liveNum && liveNum < pool->maxNum) {
			pthread_mutex_lock(&pool->mutexPool);
			int counter = 0;
			for (int i = 0; i < pool->maxNum && counter < NUMBER
				&& pool->liveNum < pool->maxNum; ++i) {
				if (pool->workerThreadsId[i] == 0) {
					counter++;
					pthread_create(&pool->workerThreadsId[i], NULL, worker, pool);
					pool->liveNum++;
				}
			}
			pthread_mutex_unlock(&pool->mutexPool);
		}

		// 销毁线程：忙的线程 * 2 < 存活的线程数 && 存活的线程数 > 最小线程数量
		if (busyNum * 2 < liveNum && liveNum > pool->minNum) {
			pthread_mutex_lock(&pool->mutexPool);
			pool->exitNum = NUMBER;
			pthread_mutex_unlock(&pool->mutexPool);
			// 唤醒正在阻塞的工作线程，让工作的线程自杀
			for (int i = 0; i < NUMBER; i++) {
				pthread_cond_signal(&pool->notEmpty);
			}
		}
	}
	return NULL;
}

void threadExit(ThreadPool* pool)
{	
	pthread_t tid = pthread_self();
	for (int i = 0; i < pool->maxNum; ++i)
	{
		if (pool->workerThreadsId[i] == tid)
		{
			pool->workerThreadsId[i] = 0;
			printf("threadExit() called, %ld exiting...\n", tid);
			break;
		}
	}
	pthread_exit(NULL);
}
