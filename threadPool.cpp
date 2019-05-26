
/*
线程池
基本思路：
   1.创建线程池结构体
    2.管理者线程
   3.工作线程
*/
#include "threadPool.h"
#include "epoll.h"


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
	p->kill_th_number = 0;
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
		if(ret!=0){
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
	if(ret!=0){
		printf("manager pthread create error\n");
		delete p;
		exit(NULL);
	}

	return p;
}

//工作线程函数
void *workThFunc(void *arg) {
	//强制转换得到线程池指针
	struct Threadpool *pool = (struct Threadpool *)arg;
	//将自己变为分离线程
	pthread_detach(pthread_self());
	while (true) {
		pthread_mutex_lock(&(pool->self_lock));
		//直接阻塞，等待条件变量唤醒 （任务队列中有任务 或者 要杀死线程时唤醒）
		while (pool->que_now_number == 0) {
			pthread_cond_wait(&(pool->wakeup), &(pool->self_lock));

			//线程自杀
			if (pool->kill_th_number > 0) {
				printf("i kill myself\n");
				pool->now_th_number--;
				pool->kill_th_number--;
				pthread_exit(NULL);
			}
		}
		//判断任务队列中是否有任务，若没有则自杀
		 if (pool->que_now_number > 0) {
			pool->busy_th_number++;
			//从队列中取出任务并执行
			struct Quemember *task = pool->task_que.front();
			pool->task_que.pop();
			pool->que_now_number--;
			if (pool->que_now_number < pool->que_max_number)
				pthread_cond_signal(&pool->que_is_full);
			pthread_mutex_unlock(&pool->self_lock);

			//执行任务
			printf("开始处理请求 pid=%lu\n",pthread_self());
			(*task->func)(task->arg);

			pthread_mutex_lock(&pool->self_lock);
			pool->busy_th_number--;
			//如果忙碌线程数量减为零 判断线程池是否应该被销毁
			if(pool->busy_th_number==0&&pool->is_destroy){
				//唤醒销毁线程
				pthread_cond_signal(&pool->destroy_pool);
			}
			pthread_mutex_unlock(&pool->self_lock);
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
	/*	if (pool->que_now_number > 0 && pool->now_th_number > pool->busy_th_number) {
			printf("队列中任务数量 ：%d\n", pool->que_now_number);
			pthread_cond_signal(&pool->wakeup);
		}*/
		printf(" %d    %d   %d      %d\n", pool->task_que.size(), pool->que_now_number, pool->now_th_number,pool->busy_th_number);
		//如果任务队列中的线程数超过CDTHREADNUMBER且当前线程数未满 则创建线程
		if (pool->que_now_number >= CDTHREADNUMBER && pool->now_th_number + CDTHREADNUMBER < pool->max_th_number) {
			
			pthread_mutex_lock(&pool->self_lock);
			for (int i = 0; i < CDTHREADNUMBER; i++) {
				printf("create thread\n");
				pthread_t tmid;
				int ret = pthread_create(&tmid, NULL, workThFunc, (void *)pool);
				if(ret!=0){
					printf("add new thread error\n");
					break;
				}
				pool->now_th_number++;
			}
			pthread_mutex_unlock(&pool->self_lock);
		}
		//如果空闲线程数过多且高于最低线程数，则下令让线程自杀
		if (pool->now_th_number > 2 * pool->busy_th_number && pool->now_th_number > pool->min_th_number) {
			//自杀线程时上锁
			
			pthread_mutex_lock(&pool->self_lock);
			pool->kill_th_number = CDTHREADNUMBER;
			pthread_mutex_unlock(&pool->self_lock);
			for(int i=0;i<CDTHREADNUMBER;i++){
				//唤醒线程函数
				printf("===================唤醒自杀线程============\n");
				pthread_cond_signal(&pool->wakeup);
				sleep(1);
			}
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
			printf("task add wait\n");
			pthread_cond_wait(&pool->que_is_full, &pool->self_lock);
		}
		//被唤醒后创建任务结构体并向线程池任务队列中添加任务
		struct Quemember *tmp_task = new Quemember();//===============================try  catch以防内存不足============================
		tmp_task->arg = arg;
		tmp_task->func = func;
		pool->task_que.push(tmp_task);
		pool->que_now_number++;
		printf("wakeup\n");
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