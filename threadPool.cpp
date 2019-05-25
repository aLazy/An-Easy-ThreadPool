
/*
线程池
基本思路：
   1.创建线程池结构体
    2.管理者线程
   3.工作线程
*/
#include "threadPool.h"
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


	pthread_cond_t wakeup;          //条件变量 唤醒线程
	pthread_cond_t que_is_full;           //条件变量 队列是否已满
	int que_max_number;                       //队列最大值
	int que_now_number;                       //队列当前值

	std::queue<struct Quemember *>task_que;     //任务队列


	bool is_destroy;			//是否销毁线程池
	pthread_cond_t destroy_pool; //销毁线程池
};


//创建线程池

struct Threadpool *createThreadPool(int min) {
	//初始化线程池
	struct Threadpool *p = new Threadpool(); //========================应该加一个try catch判断线程池是否创建成功================
	p->min_th_number = min;
	p->now_th_number = min;
	p->max_th_number = THREADNUMBER;
	p->busy_th_number = 0;
	p->que_max_number = QUEUENUMBER;
	p->que_now_number = 0;
	p->is_destroy = false;
	//  th_Array[THREADNUMBER]={0};
	
	pthread_cond_init(&p->destroy_pool,NULL);

	pthread_mutex_init(&p->self_lock, NULL);
	pthread_mutex_init(&p->work_lock, NULL);

	pthread_cond_init(&p->wakeup, NULL);
	pthread_cond_init(&p->que_is_full, NULL);

	//创建最小线程数线程
	for (int i = 0; i < min; i++) {
		pthread_t thid;
		int ret = pthread_create(&thid, NULL, workThFunc, (void *)p); //在线程中变为分离状态
		if(ret<0){
			printf("worker  pthread create error\n");
			delete p;
			exit(NULL);
		}
	}
	//创建管理者线程
	pthread_t managethid;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	int ret = pthread_create(&managethid, &attr, managerThFunc, (void *)p);
	if(ret<0){
		printf("manager pthread create error\n");
		delete p;
		exit(NULL);
	}

	//创建任务队列添加线程
	return p;
}

//工作线程函数
void *workThFunc(void *arg) {
	//强制转换得到线程池指针
	struct Threadpool *pool = (struct Threadpool *)arg;
	//将自己变为分离线程
	pthread_detach(pthread_self());
	while (true) {
		pthread_mutex_lock(&(pool->work_lock));
		//直接阻塞，等待条件变量唤醒 （任务队列中有任务 或者 要杀死线程时唤醒）
		pthread_cond_wait(&(pool->wakeup), &(pool->work_lock));

		//判断任务队列中是否有任务，若没有则阻塞
		if (pool->que_now_number > 0) {
			pthread_mutex_lock(&pool->self_lock);
			pool->busy_th_number++;
			//从队列中取出任务并执行
			struct Quemember *task = pool->task_que.front();
			pool->task_que.pop();
			pool->que_now_number--;
			pthread_mutex_unlock(&pool->self_lock);

			//执行任务
			task->func(task->arg);

			pthread_mutex_lock(&pool->self_lock);
			pool->busy_th_number--;
			//如果忙碌线程数量减为零 判断线程池是否应该被销毁
			if(pool->busy_th_number==0&&pool->is_destroy){
				//唤醒销毁线程
				pthread_cond_signal(&pool->destroy_pool);
			}
			pthread_mutex_unlock(&pool->self_lock);
		}

		//线程自杀
		else {
			pool->now_th_number--;
			pthread_exit(NULL);
		}
		pthread_mutex_unlock(&(pool->work_lock));
	}
	return NULL;
}

//管理者线程
void *managerThFunc(void *arg) {
	//强制转换得到线程池指针
	struct Threadpool *pool = (struct Threadpool *)arg;
	while (true) {
		//每SLEEPTIME秒醒来检擦一次
		sleep(SLEEPTIME);
		//如果任务队列中有任务且有空闲线程则分配线程完成
		if (pool->que_now_number > 0 && pool->now_th_number > pool->busy_th_number) {
			pthread_cond_signal(&pool->wakeup);
		}
		//如果任务队列中的线程数超过CDTHREADNUMBER且当前线程数未满 则创建线程
		if (pool->que_now_number >= CDTHREADNUMBER && pool->now_th_number + CDTHREADNUMBER < pool->max_th_number) {
			pthread_mutex_lock(&pool->self_lock);
			for (int i = 0; i < CDTHREADNUMBER; i++) {
				pthread_t tmid;
				int ret = pthread_create(&tmid, NULL, workThFunc, (void *)pool);
				if(ret<0){
					printf("add new thread error\n");
					break;
				}
				pool->now_th_number++;
			}
			pthread_mutex_unlock(&pool->self_lock);
		}
		//如果空闲线程数过多且高于最低线程数，则下令让线程自杀
		if (pool->now_th_number - pool->busy_th_number > 2 * CDTHREADNUMBER && pool->now_th_number > pool->min_th_number) {
			//自杀线程时上锁
			pthread_mutex_lock(&pool->work_lock);
			for (int i = 0; i < CDTHREADNUMBER; i++) {
				//唤醒线程函数
				pthread_cond_signal(&pool->wakeup);
			}
			pthread_mutex_unlock(&pool->work_lock);
		}
	}
	return NULL;
}

//任务队列添加线程 该函数应该在主线程中进行 监听客户端事件
void taskAddThread(void *p , void*(*func)(void *arg), void *arg) {
	struct Threadpool *pool = (struct Threadpool *)p;
	pthread_mutex_lock(&pool->self_lock);
	//如果任务队列已满则阻塞
	while (pool->que_now_number == pool->que_max_number) {
		pthread_cond_wait(&pool->que_is_full, &pool->self_lock);
	}
	//被唤醒后创建任务结构体并向线程池任务队列中添加任务
	struct Quemember *tmp_task = new Quemember();//===============================try  catch以防内存不足============================
	tmp_task->arg = arg;
	tmp_task->func = func;
	pool->task_que.push(tmp_task);
	pool->que_now_number++;
	pthread_cond_signal(&pool->wakeup);
	pthread_mutex_unlock(&pool->self_lock);
}

//释放并销毁线程池
int destroyPool(void *p) {
	struct Threadpool *pool = (struct Threadpool *)p;
	if(pool==NULL)
		return -1;
	//对池上锁
	pthread_mutex_lock(&pool->self_lock);
	pool->is_destroy=true;
	//默认为0表示销毁失败
	int result=0;
	//清空池中还未执行的任务并将队列任务的客容量设为0 防止队列控制线程接收新任务
	pool->que_max_number=0;
	while ((pool->task_que).size()) {
		delete pool->task_que.front();
		pool->task_que.pop();
	}
	//等待池中忙碌线程执行完毕   还可使用睡眠轮询等待但比较消耗cpu且不那么及时
	pthread_cond_wait(&pool->destroy_pool,&pool->self_lock);
	//唤醒线程自杀
	for (int i = 0; i < pool->now_th_number; i++) {
		pthread_cond_signal(&pool->wakeup);
	}
	//销毁池中成员
	pthread_mutex_lock(&pool->work_lock);
	pthread_mutex_destroy(&pool->work_lock);
	pthread_cond_destroy(&pool->wakeup);
	pthread_cond_destroy(&pool->que_is_full);
	//销毁成功
	result=1;
	pthread_mutex_destroy(&pool->self_lock);
	delete pool;
	pool = NULL;

	return result;
}
//