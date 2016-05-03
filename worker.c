#include "pool.h"
#include "config.h"
/*the static function declare*/
static void * worker_routline(void * arg);
static void * worker_manage(void * arg);
static void * distribute_task(void * arg);
static struct Gthread_pool_worker * search_idle_worker(struct Gthread_pool * pool);
static int add_worker(struct Gthread_pool_worker * new_worker, struct Gthread_pool * pool);
static int add_task(struct Gthread_pool_task * task, struct Gthread_pool * pool, void * (*proccess)(void * arg), void * arg);
static struct Gthread_pool_worker * get_worker_by_id(pthread_t id, struct Gthread_pool * pool);
static void sig_usr1_handler(int signum);
static float workers_usage();
static int del_worker(struct Gthread_pool_worker * worker_to_del, struct Gthread_pool * pool);

static struct timeval delay = {0, SLEEP_TIME}; 
static const float LODE_GATE = 0.2;

/*******************************************************
 * name:Gthread_pool_init
 * des:this func will init Gthread_pool struct 
 * para1: the pointer point to a Gthread_pool struct
 * para2:max number of tasks
 * para3:max number of workers
 * para4:min number of workers
 * return: state, SUCCESS or FAILURE
 * ****************************************************/
int Gthread_pool_init(struct Gthread_pool * pool, int max_tasks, int max_workers, int min_workers)
{
	assert(pool);
	pool->max_tasks = max_tasks;
	pool->max_workers = max_workers;
	pool->min_workers = min_workers;
	pool->mutex_data.worker_num = pool->min_workers;
	pool->mutex_data.task_num = 0;
	
	pthread_mutex_init(&(pool->info_lock), NULL);
#if DEBUG == 1
	pthread_mutex_init(&(pool->IO_lock), NULL);
#endif

	sem_init(&(pool->surplus_task_num), 0, 0);

	INIT_LIST_HEAD(&(pool->task_list));
	INIT_LIST_HEAD(&(pool->workers));
	
	for(int i = 0; i < pool->min_workers; i++)//initial numbers of worker
	{
		struct Gthread_pool_worker * tmp = (struct Gthread_pool_worker *)malloc(sizeof(struct Gthread_pool_worker));
		pthread_mutex_lock(&(pool->info_lock));
		add_worker(tmp, pool);
		pthread_mutex_unlock(&(pool->info_lock));
	}

	pthread_create(&(pool->manage_worker), NULL, worker_manage, (void *)pool);//create the manage thread
	pthread_create(&(pool->task_distribute_worker), NULL, distribute_task, (void *)pool);//create distibute task thread

	return SUCCESS;
}

/************************************************************************
 * name:add_task
 * des:add a task to the task list in pool
 * para1:a pointer piont to Gthread_pool_task struct'
 * para2:a pointer point to Gthread_pool
 * para3:a pointer point to the proccess function of the task
 * para4:a pointer point to the arg of the process function
 * return :state
 * **********************************************************************/
int add_task(struct Gthread_pool_task * task, struct Gthread_pool * pool, void * (*proccess)(void * arg), void * arg)
{
	assert(task);
	assert(pool);	

	task->proccess = proccess;
	task->arg = arg;
	list_add_tail(&(task->link_node), &(pool->task_list));
	return SUCCESS;
}

/************************************************************************
 * name:add_worker
 * des:add a idle worker to the worker list in pool
 * para1:a pointer piont to Gthread_pool_worker struct'
 * para2:a pointrt point to Gthread_pool
 * return :the pointer piont to new worker
 * **********************************************************************/
int add_worker(struct Gthread_pool_worker * new_worker, struct Gthread_pool * pool)
{
	int err;
	assert(new_worker);
	assert(pool);

	new_worker->state = BOOTING;
	new_worker->routline_args.pool = pool;
	new_worker->routline_args.this_worker = new_worker;
	pthread_mutex_init(&(new_worker->worker_lock), NULL);
	pthread_cond_init(&(new_worker->worker_cond), NULL); 
	pthread_mutex_init(&(new_worker->boot_lock), NULL);
	pthread_cond_init(&(new_worker->boot_cond), NULL); 

	pthread_mutex_lock(&(new_worker->boot_lock));

	err = pthread_create(&(new_worker->id), NULL, worker_routline, &(new_worker->routline_args));

	if(err != 0)
	  return FAILURE;
	
	while(new_worker->state == BOOTING)
		pthread_cond_wait(&(new_worker->boot_cond), &(new_worker->boot_lock));
	pthread_mutex_unlock(&(new_worker->boot_lock));

	list_add_tail(&(new_worker->link_node), &(pool->workers));
	return SUCCESS;
}

/* ********************************************************************
 *name:distribute_task
 *des: distribute the tasks to idle thread
 *para:a pointer point to Gthread_pool
 *return: void *
 * *******************************************************************/
void * distribute_task(void * arg)
{
	pthread_detach(pthread_self());
	assert(arg);

	struct Gthread_pool * pool = (struct Gthread_pool *)arg;
	pthread_detach(pthread_self());//make this thread unjoinable
	struct Gthread_pool_worker * idle_worker;
	struct Gthread_pool_task * task_to_distribute;
	struct list_head * pos;

#if DEBUG == 1
	pthread_mutex_lock(&(pool->IO_lock));
	printf("Now start to schedule!\n");
	pthread_mutex_unlock(&(pool->IO_lock));
#endif
	while(1)
	{
		sem_wait(&(pool->surplus_task_num));//get a task ,if there is no task to process, this thread will be blocked
		if(pool->flag == SHUTDOWN)
			pthread_exit(NULL);
		pos = pool->task_list.next;
		task_to_distribute = list_entry(pos, struct Gthread_pool_task, link_node);//get a task pointer
		idle_worker = search_idle_worker(pool);
		if(idle_worker != NULL)//find a idle worker, distribute a task to it
		{
			pthread_mutex_lock(&(pool->info_lock));
			(pool->mutex_data.task_num)--;
			list_del(pos);//remove the distributed task from task list
			pthread_mutex_unlock(&(pool->info_lock));

			pthread_mutex_lock(&(idle_worker->worker_lock));
			idle_worker->worker_task = task_to_distribute->proccess;
			idle_worker->worker_task_arg = task_to_distribute->arg;
			pthread_cond_signal(&(idle_worker->worker_cond));//send a sigal to make this worker start to work
			pthread_mutex_unlock(&(idle_worker->worker_lock));
			free(task_to_distribute);
			continue;
		}
		else
		{
			if(pool->mutex_data.worker_num < pool->max_workers)//add a new worker
			{
				idle_worker = (struct Gthread_pool_worker *)malloc(sizeof(struct Gthread_pool_worker));
				if( NULL == idle_worker )
				{
#if DEBUG == 1
					pthread_mutex_lock(&(pool->IO_lock));
					printf("malloc a new worker in distribute_task function failed!");
					pthread_mutex_unlock(&(pool->IO_lock));
#endif
					exit(15);
				}
				pthread_mutex_lock(&(pool->info_lock));
			    if( FAILURE == add_worker(idle_worker, pool) )//add worker failed continue
				{
					pthread_mutex_unlock(&(pool->info_lock));
					sem_post(&(pool->surplus_task_num));
					free(idle_worker);
					continue;
				}
				pool->mutex_data.worker_num++;
				pool->mutex_data.task_num--;
				list_del(pos);
				pthread_mutex_unlock(&(pool->info_lock));

				pthread_mutex_lock(&(idle_worker->worker_lock));
				idle_worker->worker_task = task_to_distribute->proccess;
				idle_worker->worker_task_arg = task_to_distribute->arg;
				idle_worker->state = BUSY;
				pthread_cond_signal(&(idle_worker->worker_cond));//send a sigal to make this worker start to work
				pthread_mutex_unlock(&(idle_worker->worker_lock));
				free(task_to_distribute);
				continue;
			}

			sem_post(&(pool->surplus_task_num)); //can not proccess this task now, wait for nexit time 
			select(0, NULL, NULL, NULL, &delay);
			continue;
		}
	}	
}

/* ********************************************************************
 *name:search_idle_worker
 *des: search for the worker list, and find a idle worker, the idle worker found will be set busy
 *para:a pointer piont to a Gthread_pool struct
 *return: a pointer piont to a Gthread_pool_worker struct, if none, return NULL 
 * *******************************************************************/
struct Gthread_pool_worker * search_idle_worker(struct Gthread_pool * pool)
{
	assert(pool);
	struct list_head * pos;
    struct Gthread_pool_worker * tmp_worker;
	
	pthread_mutex_lock(&(pool->info_lock));
	list_for_each(pos, &(pool->workers))
	{
		tmp_worker = list_entry(pos, struct Gthread_pool_worker, link_node);
		pthread_mutex_lock(&(tmp_worker->worker_lock));
		if(tmp_worker->state == READY)//if find a idle worker, return it's pointer
		{
			tmp_worker->state = BUSY;//this woker will be woking soon, let it' flag become BUSY
			pthread_mutex_unlock(&(tmp_worker->worker_lock));
			pthread_mutex_unlock(&(pool->info_lock));
			return tmp_worker;
		}

		else
			pthread_mutex_unlock(&(tmp_worker->worker_lock));//if this worker is busy, find the next one
	}
	pthread_mutex_unlock(&(pool->info_lock));
	return NULL;//no idle worker, return null
}

/********************************************************************************
 * name:worker_routline
 * des:the thread funtion for every worker
 * para:a void pointer
 * return: a void pointer
 * *****************************************************************************/
void * worker_routline(void * arg)
{
	pthread_detach(pthread_self());//worker thread unjoinable
	struct Gthread_pool * pool = (*((struct Gthread_pool_worker_routline_args *)arg)).pool;
	struct Gthread_pool_worker * this_worker = (*((struct Gthread_pool_worker_routline_args *)arg)).this_worker;
	
	sigset_t block_set;
	sigemptyset(&block_set);
	sigaddset(&block_set, SIGUSR1);
	signal(SIGUSR1, sig_usr1_handler);	


#if DEBUG == 1
	pthread_sigmask(SIG_BLOCK, &block_set, NULL);
	pthread_mutex_lock(&(pool->IO_lock));
	printf("Now enter the worker thread: ID %ld\n", pthread_self());
	pthread_mutex_unlock(&(pool->IO_lock));
	pthread_sigmask(SIG_UNBLOCK, &block_set, NULL);
#endif	
	if(this_worker == NULL)
	{
#if DEBUG == 1
		pthread_sigmask(SIG_BLOCK, &block_set, NULL);
		pthread_mutex_lock(&(pool->IO_lock));
		printf("a thread can not get his info by id, his id is: %ld\n",pthread_self());
		pthread_mutex_unlock(&(pool->IO_lock));
		pthread_sigmask(SIG_UNBLOCK, &block_set, NULL);
#endif
		exit(16);
	}
	while(1)
	{
		pthread_mutex_lock(&(this_worker->worker_lock));	

		if(this_worker->state == BOOTING)//to confirm a worker has ready to accept task when the function add_worker has been finished
		{
			pthread_mutex_lock(&(this_worker->boot_lock));
			this_worker->state = READY;
			pthread_cond_signal(&(this_worker->boot_cond));
			pthread_mutex_unlock(&(this_worker->boot_lock));
		}

		pthread_cond_wait(&(this_worker->worker_cond), &(this_worker->worker_lock));//wait the sigal to process task
		pthread_mutex_unlock(&(this_worker->worker_lock));

		if(pool->flag == SHUTDOWN)//pool is shutting
		{
#if DEBUG == 1
			pthread_mutex_lock(&(pool->IO_lock));
			printf("the worker thread, id: %ld will eixt!\n",pthread_self()); 
			pthread_mutex_unlock(&(pool->IO_lock));
#endif
			pthread_mutex_lock(&(pool->info_lock));
			pthread_mutex_destroy(&(this_worker->boot_lock));
			pthread_mutex_destroy(&(this_worker->worker_lock));
			pthread_cond_destroy(&(this_worker->boot_cond));
			pthread_cond_destroy(&(this_worker->worker_cond));
			
			list_del(&(this_worker->link_node));
			free(this_worker);
			pool->mutex_data.worker_num--;
			pthread_mutex_unlock(&(pool->info_lock));

			pthread_exit(NULL);
		}

		pthread_sigmask(SIG_BLOCK, &block_set, NULL);
		(*(this_worker->worker_task))(this_worker->worker_task_arg);//process the task
		pthread_sigmask(SIG_UNBLOCK, &block_set, NULL);
		
		pthread_mutex_lock(&(this_worker->worker_lock));
		this_worker->state = READY;
		pthread_mutex_unlock(&(this_worker->worker_lock));

#if DEBUG == 1
		pthread_sigmask(SIG_BLOCK, &block_set, NULL);
		pthread_mutex_lock(&(pool->IO_lock));
		printf("The worker %ld has finish a task!\n", pthread_self()); 
		pthread_mutex_unlock(&(pool->IO_lock));
		pthread_sigmask(SIG_UNBLOCK, &block_set, NULL);
#endif
	}
}



/********************************************************************************
 * name:get_worker_by_id
 * des:find a worker through the thread id
 * para1:thread id 
 * para2:a pointer point to Gthread pool
 * return: a pointer point to the worker has this id, if can not find, return NULL
 * *****************************************************************************/
struct Gthread_pool_worker * get_worker_by_id(pthread_t id, struct Gthread_pool * pool)
{
	assert(pool);
	
	struct list_head * pos;
    struct Gthread_pool_worker * tmp_worker;

	pthread_mutex_lock(&(pool->info_lock));
	list_for_each(pos, &(pool->workers))
	{
		tmp_worker = list_entry(pos, struct Gthread_pool_worker, link_node);
		if(id == tmp_worker->id)
		{
			pthread_mutex_unlock(&(pool->info_lock));
			return tmp_worker;
		}
	}
	pthread_mutex_unlock(&(pool->info_lock));
	return NULL;
}



/*************************************************************************************
 * name: get_pool_usage
 * des: to get the usage of the workers , notic! when the number of wokers now is not bigger than min_worker_num, this function will return 1;
 * para: a ponter point to a thread pool
 * return: float type, the usage of the workers
 * **********************************************************************************/
float get_pool_usage(struct Gthread_pool * pool)
{
	int total, busy_num = 0;
	struct list_head * pos;
	struct Gthread_pool_worker * a_worker;
	pthread_mutex_lock(&(pool->info_lock));

	total = pool->mutex_data.worker_num;
	if(total <= pool->min_workers)
	{
		pthread_mutex_unlock(&(pool->info_lock));
		return	(float)1;
	}

	list_for_each(pos, &(pool->workers))//for_each the worker list and calculate the usage
	{
		a_worker = list_entry(pos, struct Gthread_pool_worker, link_node);
		pthread_mutex_lock(&(a_worker->worker_lock));
		if(a_worker->state == BUSY)
			busy_num++;
		pthread_mutex_unlock(&(a_worker->worker_lock));
	}

	pthread_mutex_unlock(&(pool->info_lock));
	return ((float)busy_num)/((float)total);
}

/********************************************************************************
 * name:del_worker:
 * des:delet a worker in thread pool
 * para:a pointer point to a worker
 * return: SUCCESS or FAILURE
 * *****************************************************************************/
int del_worker(struct Gthread_pool_worker * worker_to_del, struct Gthread_pool * pool)
{
	pthread_kill(worker_to_del->id, SIGUSR1);

	while(0 == pthread_kill(worker_to_del->id, 0) )
		select(0, NULL, NULL, NULL, &delay);
	pthread_mutex_destroy(&(worker_to_del->boot_lock));
	pthread_mutex_destroy(&(worker_to_del->worker_lock));
	pthread_cond_destroy(&(worker_to_del->boot_cond));
	pthread_cond_destroy(&(worker_to_del->worker_cond));
#if DEBUG == 1
	pthread_mutex_lock(&(pool->IO_lock));
	printf("The worker %ld has been delete!\n", worker_to_del->id); 
	pthread_mutex_unlock(&(pool->IO_lock));
#endif
	
	list_del(&(worker_to_del->link_node));
	return SUCCESS; 
} 
/***************************************************************************
 * name:sig_usr1_handler
 * des: to handle the sigal SIGUSR1. here this signal will made the thread who capture it exit.
 * para:sig number
 * return: void
 * *************************************************************************/
void sig_usr1_handler(int signum)
{
	pthread_exit(NULL);
}

/* *************************************************************************
 *name:worker_manage
 *description: the manager will reduce the number of workers when usage is lower than lode_gate
 *void pointer(this pointer will be translated to a Gthread pool type pointer)
 *return: void pointer(it will be NULL)
 * *************************************************************************/
void * worker_manage(void * arg)
{
	assert(arg);
	pthread_detach(pthread_self());
	struct Gthread_pool * pool = (struct Gthread_pool *)arg;
	struct Gthread_pool_worker * worker_to_del;
	struct Gthread_pool_task * task_to_del;
	struct list_head * pos;
	sleep(1);
	while(1)
	{
		if(pool->flag == SHUTDOWN)//now the pool should be closed
		{
			sem_post(&(pool->surplus_task_num));//post this sem in case that distribute thread is blocked because of there is no task
			while(0 == pthread_kill(pool->task_distribute_worker, 0) )//wait the distribute thread exit;
				select(0, NULL, NULL, NULL, &delay);

			pthread_mutex_lock(&(pool->info_lock));

			while( (pos = pool->workers.next) != &(pool->workers))
			{
				worker_to_del = list_entry(pos, struct Gthread_pool_worker, link_node);
				del_worker(worker_to_del, pool);
				free(worker_to_del);
				pool->mutex_data.worker_num--;
			}
			
			list_for_each(pos, &(pool->task_list))//delete all tasks
			{
				task_to_del = list_entry(pos, struct Gthread_pool_task, link_node);
				list_del(pos);
				free(task_to_del);
				pool->mutex_data.task_num--;
			}
			pthread_mutex_unlock(&(pool->info_lock));

			pthread_exit(NULL);
		}

		if(get_pool_usage(pool) < LODE_GATE)
		{
			worker_to_del = search_idle_worker(pool);
			if(worker_to_del == NULL)
			{
				sleep(1);
				continue;
			}
			pthread_mutex_lock(&(pool->info_lock));
			del_worker(worker_to_del, pool);
			free(worker_to_del);
			pool->mutex_data.worker_num--;
			pthread_mutex_unlock(&(pool->info_lock));
		}
		sleep(1);
	}
}

/* ***********************************************************************************************************
 *name:close_pool
 *description:close the pool
 *para1: a pointer point to a pool
 *return SUCCESS or FAILURE
 *************************************************************************************************************/
int close_pool(struct Gthread_pool * pool)
{
	assert(pool);
	pool->flag = SHUTDOWN;
#if DEBUG == 1
		pthread_mutex_lock(&(pool->IO_lock));
		printf("The Gthread pool will close!\n"); 
		pthread_mutex_unlock(&(pool->IO_lock));
#endif
	while(0 == pthread_kill(pool->manage_worker, 0) )//wait the manage thread exit;
		select(0, NULL, NULL, NULL, &delay);
	sem_destroy(&(pool->surplus_task_num));
	pthread_mutex_destroy(&(pool->IO_lock));
	pthread_mutex_destroy(&(pool->info_lock));
	
	return SUCCESS; 
}

/* ***********************************************************************************************************
 *name:add_job
 *description:add a task to this pool 
 *para1: a pointer point to a pool
 *para2:a pointer point to a fucntion like this: void * (* func)(void * arg)
 *return SUCCESS or FAILURE
 *************************************************************************************************************/
int add_job(struct Gthread_pool * pool, void * (* job)(void * arg), void * arg)
{
	assert(pool);
	assert(arg);
	struct Gthread_pool_task * task_to_add;
	if(pool->flag == SHUTDOWN)
	{
		return FAILURE;
	}

	pthread_mutex_lock(&(pool->info_lock));
	task_to_add = (struct Gthread_pool_task *)malloc(sizeof(struct Gthread_pool_task));
	if(task_to_add == NULL)
	{
		exit(34);
	}
	add_task(task_to_add, pool, job, arg);
	pool->mutex_data.task_num++;
	sem_post(&(pool->surplus_task_num));
	pthread_mutex_unlock(&(pool->info_lock));

	return SUCCESS;
}
