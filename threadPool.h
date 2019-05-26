#ifndef __THREADPOOL__
#define __THREADPOOL__
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <queue>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#define THREADNUMBER 100
#define CDTHREADNUMBER 10
#define QUEUENUMBER   50
#define SLEEPTIME 10

//任务队列中的元素
struct Quemember {
	void *arg;     //任务函数的参数
	void *(*func)(void *arg);  //任务函数  及此任务要执行的操作
};

//线程池结构体
struct Threadpool {
	pthread_mutex_t self_lock;        //修改此结构体时上锁
	pthread_mutex_t work_lock;        //取/放线程时上锁
	//  int th_Array[THREADNUMBER];       //线程池数组
	int min_th_number;                //池中最小线程数
	int max_th_number;                //池中最大线程数
	int now_th_number;                //池中当前线程数量
	int busy_th_number;               //正在工作的线程数量

	int  kill_th_number;
	pthread_cond_t wakeup;          //条件变量 唤醒线程
	pthread_cond_t que_is_full;           //条件变量 队列是否已满
	int que_max_number;                       //队列最大值
	int que_now_number;                       //队列当前值

	std::queue<struct Quemember *>task_que;     //任务队列


	bool is_destroy;			//是否销毁线程池
	pthread_cond_t destroy_pool; //销毁线程池
};
struct Threadpool *createThreadPool(int min);

void *workThFunc(void *arg);

void *managerThFunc(void *arg);

void taskAddThread(void *p , void*(*func)(void *arg), void *arg);

int destroyPool(void *p);

#endif