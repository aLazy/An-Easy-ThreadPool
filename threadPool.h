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

//��������е�Ԫ��
struct Quemember {
	void *arg;     //�������Ĳ���
	void *(*func)(void *arg);  //������  ��������Ҫִ�еĲ���
};

//�̳߳ؽṹ��
struct Threadpool {
	pthread_mutex_t self_lock;        //�޸Ĵ˽ṹ��ʱ����
	pthread_mutex_t work_lock;        //ȡ/���߳�ʱ����
	//  int th_Array[THREADNUMBER];       //�̳߳�����
	int min_th_number;                //������С�߳���
	int max_th_number;                //��������߳���
	int now_th_number;                //���е�ǰ�߳�����
	int busy_th_number;               //���ڹ������߳�����

	int  kill_th_number;
	pthread_cond_t wakeup;          //�������� �����߳�
	pthread_cond_t que_is_full;           //�������� �����Ƿ�����
	int que_max_number;                       //�������ֵ
	int que_now_number;                       //���е�ǰֵ

	std::queue<struct Quemember *>task_que;     //�������


	bool is_destroy;			//�Ƿ������̳߳�
	pthread_cond_t destroy_pool; //�����̳߳�
};
struct Threadpool *createThreadPool(int min);

void *workThFunc(void *arg);

void *managerThFunc(void *arg);

void taskAddThread(void *p , void*(*func)(void *arg), void *arg);

int destroyPool(void *p);

#endif