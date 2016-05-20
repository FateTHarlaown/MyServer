#include <netinet/in.h>
#include <iostream>
#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include "server.h"
#include <unistd.h>
#include <sys/epoll.h>
#include "timer.h"
#include <assert.h>
#include <signal.h>
#include "Glist.h" 
#define METHOD_LEN 255
#define PATH_LEN 255
#define URL_LEN 255
#define VERSION_LEN 50
#define MAXEVENTS 255
static int BACKLOG = 100000;
static int pipefd[2];
static server_conf server_config;
static struct Gthread_pool pool;
static int get_line(int sock, char *buf, int size);
static void * client_service(void * arg);
static void doGetMethod(int client_fd, char * url, char * version);
static void unimplemented(int client);
static void not_found(int client);
static void headers(int client);
static void cat(int client, FILE *resource);
static void file_serve(int client_fd, char * filename);
static void cannot_execute(int client);
static void execute_cgi(int client, const char *path, const char *method, const char *query_string);
static int setNoBlock(int fd);
static void sig_alarm_handle(int sig);
void sig_int_handle(int sig);
void close_client(int client_fd);
static list_head timer_list;

/* *************************************************************
 *name:request_handle
 *para:void *(thread type, no use )
 *return:void *
 ************************************************************ */
void request_handle(struct Gthread_pool * pool)
{
	signal(SIGPIPE, SIG_IGN);
	signal(SIGALRM, sig_alarm_handle);
	signal(SIGINT, sig_int_handle);
	alarm(2);
	int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(listen_fd == -1)
	{
		//error handle
#if DEBUG == 1
		printf("Create listen_fd failed!"); 
		return ;
#endif
	}
	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(server_para.ListenPort);
	bind(listen_fd, (sockaddr*)&server_addr, sizeof(server_addr));
	char * ptr;
	if( (ptr = getenv("LISTENQ")) != NULL)
	{
#if DEBUG == 1
	printf("Get listen queue lenth from kernel:%d\n", BACKLOG); 
#endif
		BACKLOG = atoi(ptr);
	}
	else
		//printf("can not get listen queue from keinel\n");
	listen(listen_fd, BACKLOG);

#if DEBUG == 1
	printf("Ready to accept the client request!\n");
#endif
	
	int epoll_fd = epoll_create(5);
	assert(epoll_fd != -1);
	for(int i = 0; i < MAX_FD; i++)
	{
		timers[i] = NULL;
	}

	INIT_LIST_HEAD(&timer_list);
#if DEBUG == 1
	printf("&timer_list:%p\n", &timer_list);
#endif
	pthread_mutex_init(&timer_lock, NULL);
	int r = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
	assert(r != -1);
	setNoBlock(pipefd[1]);
	add_event(epoll_fd, listen_fd, DATA_IN);
	add_event(epoll_fd, pipefd[0], DATA_IN);
	epoll_event events[MAXEVENTS];
	int overtime_ms = -1;
	bool sys_tick = false;
	bool run = true;

	while(run == true)
	{
		int ret = epoll_wait(epoll_fd, events, MAXEVENTS, overtime_ms);
		if(ret < 0 && errno != EINTR)
		{
#if DEBUG == 1
			printf(" epoll wait failure: %d\n", ret);
			perror("OH epoll:");
#endif
			break;	
		}

		for(int i = 0; i < ret; i++)
		{
			if(events[i].data.fd == listen_fd)//a new client 
			{
				int client_fd = accept(listen_fd, NULL, 0);
				add_event(epoll_fd, client_fd, DATA_IN);
#if DEBUG == 1
				printf("Now a new client is coming ! his fd: %d\n",client_fd);
#endif
				timer_type * new_timer = (timer_type*)malloc(sizeof(timer_type));
				new_timer->over_time = LIFE_TIME + time(NULL);	
				new_timer->fd = client_fd;
				new_timer->callback_func = over_time_handle;
				pthread_mutex_lock(&timer_lock);
#if DEBUG == 1
				printf("add a timer, fd:%d\n",client_fd);
#endif
				timer_add(new_timer, &timer_list);
#if DEBUG == 1
				printf("new timer next:%p\n",timer_list.next->next);
				printf("new timer next:%p\n",timer_list.next->prev);
				printf("timer_list:%p\n",&timer_list);
#endif
				pthread_mutex_unlock(&timer_lock);
			}
			else if((events[i].data.fd == pipefd[0]) && (events[i].events & EPOLLIN))// we get a signal
			{
#if DEBUG == 1
				printf("Ready to accept some signals\n");
#endif
				char signals[1024];
				ret = recv(pipefd[0], signals, sizeof(signals), 0);
				if(ret == -1)
				{
					//
				}
				else if(ret == 0)
				{
					continue;
				}
				else
				{
					for(int i = 0; i < ret; i++)
					{
						switch(signals[i])
						{
							case SIGALRM:
#if DEBUG == 1
										printf("Now sys beating!\n");
#endif
										sys_tick = true;//now have only SIGALARM
										 break;
							case SIGINT:run = false;
							default: 
										 break;
						}
					}
				}
			}
			else if(events[i].events & EPOLLIN)// a client send request
			{
#if DEBUG == 1
				printf("Now a new client start to comunicate!\n");
#endif
				/* add a client task to thread pool and delete the fd */
				add_job(pool, client_service, new int(events[i].data.fd)); 
				del_event(epoll_fd, events[i].data.fd, DATA_IN);
			}
			else if(events[i].events & EPOLLRDHUP)// a client close the fd
			{
#if DEBUG == 1
				printf(" the client close the fd %d!\n", events[i].data.fd);
#endif
				close(events[i].data.fd);
				del_event(epoll_fd, events[i].data.fd, DATA_IN);
				pthread_mutex_lock(&timer_lock);
				timer_del(timers[events[i].data.fd], &timer_list);
				pthread_mutex_unlock(&timer_lock);
			}
			else
			{

#if DEBUG == 1
				printf(" something else happen!\n");
#endif
			}
		}

		if(sys_tick)//alarm happened check the timers
		{
#if DEBUG == 1
			printf("Now handle the alarm signal\n");
#endif
#if DEBUG == 1
			printf("timer_list:%p\n",&timer_list);
#endif
			pthread_mutex_lock(&timer_lock);
			sys_tick_handle(&timer_list);
			pthread_mutex_unlock(&timer_lock);
			sys_tick = false;
		}

		if(!list_empty(&timer_list))
		{
#if DEBUG == 1
			printf("Now update the overtime_ms for epoll\n");
#endif
			pthread_mutex_lock(&timer_lock);
			if(!list_empty(&timer_list))
			{
				time_t cur = time(NULL);
				list_head * pos = timer_list.next;
				timer_type * entry = list_entry(pos, timer_type, node);
				overtime_ms = (entry->over_time - cur) * 1000;
			}
			pthread_mutex_unlock(&timer_lock);
		}
		else
		  overtime_ms = -1;
			
	    
	}//while()
	pthread_mutex_destroy(&timer_lock);
	close(listen_fd);
/*  
	while(1) 
	{ 
		int client_fd = accept(listen_fd, NULL, NULL); 
		if(client_fd != -1)
		{
			add_job(pool, client_service, new int(client_fd)); 
		}
	} 
*/
}
/* ************************************************************* 
   *name:client_service 
   *para:void *(must to be convered to int * ),the data is malloced in heap, so it must be deleted in the end of the function 
   *return:void * */ 
void * client_service(void * arg) 
{ 
	int client_fd = *(int*)arg; 
	delete (int*)arg; 
#if DEBUG == 1 
	printf("Now to serve for the client , fd: %d\n", client_fd); 
#endif 
	char rcv_buffer[BUFSIZ]; 
	char method[METHOD_LEN];
	char url[URL_LEN]; 
	char version[VERSION_LEN];
	int n;
	n = get_line(client_fd, rcv_buffer, BUFSIZ);
#if DEBUG == 1
	printf("%s", rcv_buffer);
#endif
	int i = 0, j = 0;

	while(rcv_buffer[j] != ' ' && i <= METHOD_LEN-1 && j <= n)//get the method
	{
		method[i] = rcv_buffer[j];
		i++;
		j++;
	}
	method[i] = '\0';

	i = 0;
	while(rcv_buffer[j] == ' ' && j <= n)
		j++;
	while(rcv_buffer[j] != ' ' && j <= n && i < URL_LEN-1)//get the URL
	{
		url[i] = rcv_buffer[j];
		i++;
		j++;
	}
	url[i] = '\0';

	i = 0;
	while(rcv_buffer[j] == ' ' && j <= n)
		j++;
	while(rcv_buffer[j] <= n && i < VERSION_LEN-1)
	{
		version[i] = rcv_buffer[j];
		i++;
		j++;
	}
	version[i] = '\0';
	
	if(strcasecmp(method, "GET") && strcasecmp(method, "POST"))
	{
		//can not under stand the request
		unimplemented(client_fd);
		close_client(client_fd);
		return NULL;
	}

	if(strcasecmp(method, "GET") == 0)
	{
		//call GET method function
		doGetMethod(client_fd, url, version);
	}
	else if(strcasecmp(method, "POST") == 0)
	{
		//call POST function
	}

	return NULL;
}

/* **********************************************************
 *name:get_line
 *des:this funtion get a string from a socket fd.this string's  last non-void charater is '\n'(the /r/n will be translated to '\n') 
 *para1:the fd to read from
 *para2:the buffer
 *para3:buffer size
 *return:the nunber of the bytes stored(excluding null)
 * */
int get_line(int sock, char *buf, int size)
{
	int i = 0;
	char c = '\0';
	int n;

	while ((i < size - 1) && (c != '\n'))
	{
	n = recv(sock, &c, 1, 0);
	if(n > 0)
	{
		if(c == '\r')
		{
			n = recv(sock, &c, 1, MSG_PEEK);
			if((n > 0) && (c == '\n'))
			 recv(sock, &c, 1, 0);
			else
			 c = '\n';
	   }
	   buf[i] = c;
	   i++;
	}
	else
		c = '\n';
	}
	buf[i] = '\0';
 
 return(i);
}

/*****************************************************************
 * name:doGetMethod
 * des:this function will answer the request of GET method 
 *para1:the client fd
 *para2:the url buffer
 *para3:the http protocol version buffer
 *return: void
 *****************************************************************/
void doGetMethod(int client_fd, char * url, char * version)
{
	char path[BUFSIZ];
	struct stat st;
	char * query_string = NULL;
	query_string = url;
	while( *query_string != '?' && *query_string != '\0')
		query_string++;
	if(*query_string == '?')
	{
		*query_string = '\0';
		query_string++;
	}
	sprintf(path, "%s%s", server_para.DocumentRoot, url);		
	if(path[strlen(path)-1] == '/')
		strcat(path, server_para.DefaultFile);

#if DEBUG == 1
	printf("Now to find the file: %s\n", path);
#endif

	if(stat(path, &st) == -1)
	{
		//can not find 404
#if DEBUG == 1
		printf("can not find the file\n");
#endif
		not_found(client_fd);
		close_client(client_fd);
		return;
	}
	else
	{
		if(S_ISDIR(st.st_mode))
		{
			strcat(path, "/");
			strcat(path, server_para.DefaultFile);
		}
		
		if(st.st_mode & S_IXUSR || st.st_mode & S_IXGRP || st.st_mode &S_IXOTH)
		{
			//CGI server
			execute_cgi(client_fd, path, "GET", query_string); 				
			close_client(client_fd);
		}
		else
		{
			//file server
			file_serve(client_fd, path);

#if DEBUG == 1
		printf("We have send the file ,now to close fd\n");
#endif
			close_client(client_fd);
		}
	
	}
}

/* *********************************************************
 * name: unimplement
 * des:this function reply to client that the server can not under stand his request
 * para1: the fd of the client
 * return void
 * **********************************************************/
void unimplemented(int client)
{
 char buf[1024];

 sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, SERVER_STRING);
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "Content-Type: text/html\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "</TITLE></HEAD>\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "</BODY></HTML>\r\n");
 send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
void not_found(int client)
{
 char buf[1024];

#if DEBUG == 1
		printf("now send the 404 infomation!\n");
#endif
 sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, SERVER_STRING);
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "Content-Type: text/html\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "your request because the resource specified\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "is unavailable or nonexistent.\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "</BODY></HTML>\r\n");
 send(client, buf, strlen(buf), 0);
}

/* ******************************************************************
 * name:file_serve
 * des:this function send the headers an the file client requested to the client
 * para1:the fd of the client
 * para2:a FILE * pointer pointed to the file to be sent
 * return: void
 * ************************************************************************/
void file_serve(int client_fd, char * filename)
{
	char buffer[BUFSIZ];
	int n = 1;
	FILE * resource;
	buffer[0] = 'A';
	buffer[1] = '\0';
	while(n > 0 && strcmp(buffer, "\n"))//get and discard headers
	{
		get_line(client_fd, buffer, sizeof(buffer));
#if DEBUG == 1
		printf("%s", buffer);
#endif
	}
	resource = fopen(filename, "r");
	if(resource == NULL)
	{
#if DEBUG == 1
		printf("can not open the file\n");
#endif
		not_found(client_fd);
		close(client_fd);
		return;
	}
	else
	{

#if DEBUG == 1
		printf("Now send the file\n");
#endif
		headers(client_fd);
		cat(client_fd, resource);
	}
	fclose(resource);
}

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
/**********************************************************************/
void headers(int client)
{
 char buf[1024];

 strcpy(buf, "HTTP/1.0 200 OK\r\n");
 send(client, buf, strlen(buf), 0);
 strcpy(buf, "connection:keep-alive\r\n");
 send(client, buf, strlen(buf), 0);
 strcpy(buf, SERVER_STRING);
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "Content-Type: text/html\r\n");
 send(client, buf, strlen(buf), 0);
 strcpy(buf, "\r\n");
 send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
/**********************************************************************/
void cat(int client, FILE *resource)
{
 char buf[1024];

 fgets(buf, sizeof(buf), resource);
 while (!feof(resource))
 {
  send(client, buf, strlen(buf), 0);
  fgets(buf, sizeof(buf), resource);
 }
}

/**********************************************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
/**********************************************************************/
void cannot_execute(int client)
{
 char buf[1024];

 sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "Content-type: text/html\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
 send(client, buf, strlen(buf), 0);
}

/* *******************************************************************************************
 * execute the cgi script (this function need have not realize POST
 * para1:the fd of the client
 * para2:the path of the script
 * para3: the method
 * para4: query string
 * return:void*/
void execute_cgi(int client, const char *path, const char *method, const char *query_string)
{
 char buf[1024];
 int cgi_output[2];
 int cgi_input[2];
 pid_t pid;
 int status;
// int i;
 char c;
 int numchars = 1;
 //int content_length = -1;

 buf[0] = 'A'; buf[1] = '\0';
 if (strcasecmp(method, "GET") == 0)
  while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
  {
	numchars = get_line(client, buf, sizeof(buf));
#if DEBUG == 1
	printf("%s\n", buf);
#endif
  }
 else    /* POST */
 {
	 //need for next time
	cannot_execute(client);
	close(client);
	return;
 }

 //can run
 sprintf(buf, "HTTP/1.0 200 OK\r\n");
 send(client, buf, strlen(buf), 0);

 if (pipe(cgi_output) < 0) {
  cannot_execute(client);
  return;
 }
 if (pipe(cgi_input) < 0) {
  cannot_execute(client);
  return;
 }

 if ( (pid = fork()) < 0 ) {
  cannot_execute(client);
  return;
 }
 if (pid == 0)  /* child: CGI script */
 {
#if DEBUG == 1
  printf("Now to run the CGI script\n");
#endif

  char meth_env[255];
  char query_env[255];
  //char length_env[255];

  dup2(cgi_output[1], 1);
  dup2(cgi_input[0], 0);
  close(cgi_output[0]);
  close(cgi_input[1]);
  sprintf(meth_env, "REQUEST_METHOD=%s", method);
  putenv(meth_env);
  if (strcasecmp(method, "GET") == 0) {
   sprintf(query_env, "QUERY_STRING=%s", query_string);
   putenv(query_env);
  }
  else {   /* POST */
   //sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
   //putenv(length_env);
  }
  execl(path, path, NULL);
  exit(0);
 } else {    /* parent */
  close(cgi_output[1]);
  close(cgi_input[0]);
  /*  
  if (strcasecmp(method, "POST") == 0)
   for (i = 0; i < content_length; i++) {
    recv(client, &c, 1, 0);
    write(cgi_input[1], &c, 1);
   }
   */
  while (read(cgi_output[0], &c, 1) > 0)
   send(client, &c, 1, 0);

  close(cgi_output[0]);
  close(cgi_input[1]);
  close(client);
  waitpid(pid, &status, 0);
 }
}

/* *********************************************************
 * name:setNoBlock
 * des:set a file scriptor no block
 * para1:a fd
 * return: the old option
 * *********************************************************/
int setNoBlock(int fd)
{
	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);
	return fd;
}

/* *************************************************************
 * name:sig_alarm_handle
 * des:the alarm signal handler
 * para1:sigal number
 * return: void
 * ************************************************************/
void sig_alarm_handle(int sig)
{
	int save_errno = errno;
	int msg = sig;
	send(pipefd[1], (char*)&msg, 1, 0);
	errno = save_errno;
	alarm(2);
}

/* *************************************************************
 * name:sig_int_handle
 * des:the SIGINT signal handler
 * para1:sigal number
 * return: void
 * ************************************************************/
void sig_int_handle(int sig)
{
	int save_errno = errno;
	int msg = sig;
	send(pipefd[1], (char*)&msg, 1, 0);
	errno = save_errno;
}


/* **********************************************************
 * name:close_client
 * des:close a client and delete it's timer
 * para1: the fd of the client
 * return:void
 * **********************************************************/
void close_client(int client_fd)
{
	if(timers[client_fd] != NULL)
	{
		pthread_mutex_lock(&timer_lock);
		if(timers[client_fd] != NULL)
		{
			timer_del(timers[client_fd], &timer_list);
			close(client_fd);
		}
		pthread_mutex_unlock(&timer_lock);
	}
}
