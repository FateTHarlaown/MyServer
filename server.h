#ifndef _SERVER
#define	_SERVER
#include "config.h"
#define SERVER_STRING "Server:A foolish server by ZJ\r\n"

bool server_init(struct Gthread_pool * pool);
bool server_close(struct Gthread_pool * pool);
void request_handle(struct Gthread_pool * pool);

#endif
