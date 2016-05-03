#include <sys/epoll.h>
#include "config.h"

/* ************************************************************
 * name:add_event
 * des:this function add a event to the epoll queue
 * para1:the fd of the epoll handler
 * para2:the fd of event 
 * para3:event type*/
void add_event(int epoll_fd, int fd, int event_type)
{
	epoll_event e;
	e.data.fd = fd;
	switch(event_type)
	{
		case DATA_IN: e.events = EPOLLIN;
					  break;
		case DATA_OUT: e.events = EPOLLOUT;
					   break;
	}
	epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &e);
}


/* ************************************************************
 * name:del_event
 * des:this function delete a event from the epoll queue
 * para1:the fd of the epoll handler
 * para2:the fd of event 
 * para3:event type*/
void del_event(int epoll_fd, int fd, int event_type)
{
	epoll_event e;
	e.data.fd = fd;
	switch(event_type)
	{
		case DATA_IN: e.events = EPOLLIN;
					  break;
		case DATA_OUT: e.events = EPOLLOUT;
					   break;
	}
	epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &e);
}
