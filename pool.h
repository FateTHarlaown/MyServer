#ifndef HTTP_POOL
#define HTTP_POOL
#ifdef __cplusplus
extern "C" {
#endif
#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <setjmp.h>
#include "Glist.h"
#include <signal.h>
				

/* Gthread pool run flag */
enum Gthread_pool_flag {RUN, SHUTDOWN}; 

/*Gthread pool worker stae*/
enum Gthread_pool_worker_state {BUSY, READY, BOOTING};

/*the mutex data for the thread pool data*/
struct mutex_pool_data{
						int worker_num;
						int task_num;
};

/* ********************************
 *define thread pool struct
 **********************************/
struct Gthread_pool{
					struct list_head task_list;
					struct list_head workers;
					enum Gthread_pool_flag flag;//the state of the Gthread pool
					sem_t surplus_task_num;//the sem > 0, means there are some tasks to be processed
					pthread_mutex_t info_lock;
					pthread_mutex_t IO_lock;
					int max_tasks;
					int max_workers;
					int min_workers;
					pthread_t manage_worker;
					pthread_t task_distribute_worker;
					struct mutex_pool_data mutex_data;
};

/* *******************************
 *define task struct in thread pool
 * *******************************/
struct Gthread_pool_task{
						 void * (*proccess)(void * arg); 
						 void * arg;
						 struct list_head link_node;
};

/* ****************************************
 * define the worker_routlie args
 * ****************************************/
struct Gthread_pool_worker;

struct Gthread_pool_worker_routline_args{
										 struct Gthread_pool * pool;
										 struct Gthread_pool_worker * this_worker;
};
/* ****************************************
 * define worker thread struct in thread pool
 * ****************************************/
struct Gthread_pool_worker{
							pthread_t id;
							pthread_mutex_t worker_lock;
							pthread_cond_t worker_cond;
							pthread_mutex_t boot_lock;
							pthread_cond_t boot_cond;
							enum Gthread_pool_worker_state state;
							struct Gthread_pool_worker_routline_args routline_args;
							void * (*worker_task)(void * );
							void * worker_task_arg;
							struct list_head link_node;
};

#ifdef __cplusplus
}
#endif
#endif
