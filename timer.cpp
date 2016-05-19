#include "timer.h"
#include <unistd.h>
timer_type * timers[MAX_FD];
pthread_mutex_t timer_lock;

/* *****************************************************************
 * name:timer_comp
 * des:the campare function for two tmimers
 * para1:timer1
 * para2:timer2
 * return: bool 
 * ****************************************************************/
bool timer_cmp(timer_type * a, timer_type * b)
{
	if(a->over_time > b->over_time)
		return true;
	else
	  return false;
}


/* ************************************************************************
 * name:timer_add
 * des:add a timer into the list
 * para1:the list(a pointer point to struct list-head)
 * para2:the timer to add(a pointer point to a timer_type
 * return:void
 * ***********************************************************************/
void timer_add(timer_type * new_timer, struct list_head * head)
{
	if(new_timer == NULL)
		return;
	if(list_empty(head))
	{
		list_add(&(new_timer->node), head);
		timers[new_timer->fd] = new_timer;
		return;
	}

	struct list_head * pos;
	timer_type * entry;
	list_for_each(pos, head)
	{
		entry = list_entry(pos, timer_type, node);
		if(timer_cmp(entry, new_timer))// find the right place to insert the timer
		{
			__list_add(&(new_timer->node), pos->prev, pos);
			timers[new_timer->fd] = new_timer;
			return;
		}
	}
	
	if(pos == head)//the new timer is bigger than all the timer in the list
	{
		timers[new_timer->fd] = new_timer;
		list_add_tail(&(new_timer->node), head);
	}
}


/* ************************************************************************
 * name:timer_del
 * des:delete a timer into the list
 * para1:the list(a pointer point to struct list-head)
 * para2:the timer to delete(a pointer point to a timer_type
 * return:void
 * ***********************************************************************/
void timer_del(timer_type * timer_del, struct list_head * head)
{
	if( timer_del == NULL || list_empty(head))
		return;
	timers[timer_del->fd] = NULL;
	list_del(&(timer_del->node));
	free(timer_del);
}

/*****************************************************************************
 * name:sys_tick_handle
 * des:after heart beating, this function will be called to chek if some timers overtime 
 * para1: timer list
 * return: void
 * **************************************************************************/
void sys_tick_handle(struct list_head *  head)
{
	timer_type * entry;
	time_t cur = time(NULL);
	while(!list_empty(head))
	{
		entry = list_entry(head->next, timer_type, node);
		if(entry->over_time > cur)
			break;
		else
		{
			entry->callback_func((void*)&entry->fd);				
			list_del(head->next);
			free(entry);
		}
	}
}

/* *****************************************************************
 * name:overtime_handle
 * des:this function will be called by the sys_tick_handle
 * para1:void * arg, must be translated to a int*
 * return: void
 * *****************************************************************/
void over_time_handle(void * arg_fd)
{
	int fd = *(int*)arg_fd;
	close(fd);
	timers[fd] = NULL;
}
