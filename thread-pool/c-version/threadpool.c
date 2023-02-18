#include "threadpool.h"
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
// ����Ķ���
typedef struct Task {
	void* (*function)(void* arg);
	void* arg;
}Task;

// �̳߳ض���
struct ThreadPool {
	Task* taskQueue;			// �������
	int queueCapacity;			// ��������
	int queueSize;				// ��ǰ������е��������
	int queueFront;				// ��ͷȡ����
	int queueRear;				// ��β������
	
	pthread_t managetId;		// �������߳�id
	pthread_t* workerThreadsId;	// �������߳�id
	int minNum;					// ��С�߳�����
	int maxNum;					// ����߳�����
	int busyNum;				// æ���߳�����
	int liveNum;				// �����߳�����
	int exitNum;				// �����ٵ��߳�����
	pthread_mutex_t mutexPool;	// ���������̳߳صĻ�����
	pthread_mutex_t mutexBusy;	// ����busyNum�Ļ�������ʹ��ϸ���ȵĻ��������Ч��
	pthread_cond_t notFull;     // ��������ǲ�������
	pthread_cond_t notEmpty;    // ��������ǲ��ǿ���

	int shutDown;				// �Ƿ������̳߳أ�0��ʾ�����٣�1��ʾ����
};

const int NUMBER = 2; // ÿ�����/���ٵ��߳�����

ThreadPool* threadPoolCreate(int min, int max, int queueSize)
{
	ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
	do {
		if (pool == NULL) {
			printf("malloc ThreadPool fail\n");
			break;
		}

		// �������еĳ�ʼ��
		pool->taskQueue = (Task*)malloc(sizeof(Task) * queueSize);
		if (pool->taskQueue == NULL) {
			printf("malloc taskQueue fail\n");
			break;
		}
		pool->queueCapacity = queueSize;
		pool->queueSize = 0;
		pool->queueFront = 0;
		pool->queueRear = 0;

		// �̵߳ĳ�ʼ��
		pool->workerThreadsId = (pthread_t*)malloc(sizeof(pthread_t) * max);
		if (pool->workerThreadsId == NULL) {
			printf("malloc workerThreadsId fail\n");
			break;
		}
		// ��ÿ�������̳߳�ʼ��Ϊ0
		memset(pool->workerThreadsId, 0, sizeof(pthread_t) * max);
		pthread_create(&pool->managetId, NULL, manager, pool);
		for (int i = 0; i < min; i++) {
			pthread_create(&pool->workerThreadsId[i], NULL, worker, pool);
		}
		pool->minNum = min;
		pool->maxNum = max;
		pool->busyNum = 0; // �������̳߳ظճ�ʼ������δ���������������
		pool->liveNum = min;
		pool->exitNum = 0;

		// ���������ͻ������ĳ�ʼ��
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

	// ��ֹ��Դй©
	if (pool && pool->taskQueue) free(pool->taskQueue);
	if (pool && pool->workerThreadsId) free(pool->workerThreadsId);
	if (pool) free(pool);

	return NULL;
}

int threadPoolDestroy(ThreadPool* pool)
{
	if (pool == NULL)
		return -1;
	// �ر��̳߳�
	pool->shutDown = 1;
	// �������չ������߳�
	pthread_join(pool->managetId, NULL);
	// �����������������߳�
	for (int i = 0; i < pool->liveNum; i++) {
		pthread_cond_signal(&pool->notEmpty);
	}
	// �ͷŶ��ڴ�
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
		// �����������߳�
		pthread_cond_wait(&pool->notFull, &pool->mutexPool);
	}
	if (pool->shutDown) {
		pthread_mutex_unlock(&pool->mutexPool);
		return;
	}
	// �������
	pool->taskQueue[pool->queueRear].arg = arg;
	pool->taskQueue[pool->queueRear].function = func;
	pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;
	pool->queueSize++;

	// ֪ͨ�������߳�����
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
			// �����������߳�
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
		// �����������ȡ��һ������
		Task task;
		task.function = pool->taskQueue[pool->queueFront].function;
		task.arg = pool->taskQueue[pool->queueFront].arg;
		// �ƶ�ͷ�ڵ㣬ά��һ�����ζ���
		pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;
		pool->queueSize--;
		// ����
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
		// ÿ��3���Ӽ��һ��
		sleep(3);
		// ȡ����������е����������͵�ǰ����̵߳�����
		pthread_mutex_lock(&pool->mutexPool);
		int queSize = pool->queueSize;
		int liveNum = pool->liveNum;
		pthread_mutex_unlock(&pool->mutexPool);

		// ȡ��æ���̵߳�����
		pthread_mutex_lock(&pool->mutexBusy);
		int busyNum = pool->busyNum;
		pthread_mutex_unlock(&pool->mutexBusy);

		// ����̣߳���ǰ�����߳� < �̳߳ص�����߳��� && �������� > ��ǰ�����߳�
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

		// �����̣߳�æ���߳� * 2 < �����߳��� && �����߳��� > ��С�߳�����
		if (busyNum * 2 < liveNum && liveNum > pool->minNum) {
			pthread_mutex_lock(&pool->mutexPool);
			pool->exitNum = NUMBER;
			pthread_mutex_unlock(&pool->mutexPool);
			// �������������Ĺ����̣߳��ù������߳���ɱ
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
