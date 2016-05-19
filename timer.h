#ifndef _MY_TIMER
#define _MY_TIMER
#include <time.h>
#include <vector>
#include <algorithm>
#include <pthread.h>
#include "Glist.h" 

#define MAX_FD 10000
using namespace std; 
#define LIFE_TIME 10
#define SLOT_TIME 1

/* the timer struct for every client */
typedef struct timer{
	time_t over_time;
	int fd;
	void (*callback_func)(void * arg);
	list_head node;	
} timer_type;




extern timer_type * timers[MAX_FD];
extern pthread_mutex_t timer_lock;


/* *****************************************************************
 * name:timer_comp
 * des:the campare function for two tmimers
 * para1:timer1
 * para2:timer2
 * return: bool 
 * ****************************************************************/
bool timer_cmp(timer_type * a, timer_type * b);

/* ************************************************************************
 * name:timer_add
 * des:add a timer into the list
 * para1:the list(a pointer point to struct list-head)
 * para2:the timer to add(a pointer point to a timer_type
 * return:void
 * ***********************************************************************/
void timer_add(timer_type * new_timer, struct list_head * head);

/* ************************************************************************
 * name:timer_del
 * des:delete a timer into the list
 * para1:the list(a pointer point to struct list-head)
 * para2:the timer to delete(a pointer point to a timer_type
 * return:void
 * ***********************************************************************/
void timer_del(timer_type * timer_del, struct list_head * head);

/*****************************************************************************
 * name:sys_tick_handle
 * des:after heart beating, this function will be called to chek if some timers overtime 
 * para1: timer list]
 * return: void
 * **************************************************************************/
void sys_tick_handle(struct list_head *  head);

/* *****************************************************************
 * name:overtime_handle
 * des:this function will be called by the sys_tick_handle
 * para1:void * arg, must be translated to a int*
 * return: void
 * *****************************************************************/
void over_time_handle(void * arg_fd);

#endif
