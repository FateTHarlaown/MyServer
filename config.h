#ifndef SHTTPD_H 
#define	SHTTPD_H
#ifdef __cplusplus
extern "C" {
#endif
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <getopt.h>
#include <semaphore.h>
#include <string.h>
#include <assert.h>
#include "pool.h"

/*********************************
 ****** The macro define area!****
 * *******************************/
/*debug flag*/
#define DEBUG 0

/* the sleep time for distribute and manage */
#define SLEEP_TIME 2

/*define the SUCCESS and FAILED MACRO**/
#define SUCCESS 1
#define FAILURE -1 

/*the events type*/
#define	 DATA_IN 0
#define DATA_OUT 1

/**********************************
 * The data define area*
 * *******************************/

/*Sever config struct*/ 
struct server_conf{
				char CGIRoot[128];
				char DefaultFile[128];
				char DocumentRoot[128];
				char ConfigFile[128];
				int ListenPort;
				int MaxClient;
				int TimeOut;
				int InitWorkerNum;
				int MaxWoerkerNum;
};


/**********************************
 * The global data declare area*
 * *******************************/

/*********************************
 *declare sever config structure*
 *********************************/
extern struct server_conf server_para;

/**********************************
 * the function declare area*
 * *******************************/

/************************************************************************
 *name: DisplayConf
 *description: displau the sever config information
 *para:none
 *return: 1 for success, -1 for wrong
 *************************************************************************/
int DisplayConf();


/************************************************************************
 *name: GetParaFromFile
 *description: this function get the config information from the config file
 *para:the char pointer pointed to the string of name of the config file name
 *return: 1 for success, -1 for wrong
 *************************************************************************/
int GetParaFromFile(char * file);


/************************************************************************
 *name: GetParaFromCmd
 *description: get the parameter from cmd lie
 *para1:argc(same with the main function)
 *para2:argv(same with the main function)
 *return: 1 for success, -1 for wrong
 *************************************************************************/
int GetParaFromCmd(int argc, char * argv[]);

/*******************************************************
 * name:Gthread_pool_init
 * des:this func will init Gthread_pool struct 
 * para1: the pointer point to a Gthread_pool struct
 * para2:max number of tasks
 * para3:max number of workers
 * para4:min number of workers
 * return: state, SUCCESS or FAILURE
 * ****************************************************/
int Gthread_pool_init(struct Gthread_pool * pool, int max_tasks, int max_workers, int min_workers);

/* ***********************************************************************************************************
 *name:close_pool
 *description:close the pool
 *para1: a pointer point to a pool
 *return SUCCESS or FAILURE
 *************************************************************************************************************/
int close_pool(struct Gthread_pool * pool);

/* ***********************************************************************************************************
 *name:add_job
 *description:add a task to this pool 
 *para1: a pointer point to a pool
 *para2:a pointer point to a fucntion like this: void * (* func)(void * arg)
 *return SUCCESS or FAILURE
 *************************************************************************************************************/
int add_job(struct Gthread_pool * pool, void * (* job)(void * arg), void * arg);

/* ************************************************************
 * name:add_event
 * des:this function add a event to the epoll queue
 * para1:the fd of the epoll handler
 * para2:the fd of event 
 * para3:event type*/
void add_event(int epoll_fd, int fd, int event_type);

/* ************************************************************
 * name:del_event
 * des:this function delete a event from the epoll queue
 * para1:the fd of the epoll handler
 * para2:the fd of event 
 * para3:event type*/
void del_event(int epoll_fd, int fd, int event_type);
#ifdef __cplusplus
}
#endif
#endif

