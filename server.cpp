#include "server.h"

bool server_init(struct Gthread_pool * pool)
{
	Gthread_pool_init(pool, server_para.MaxClient, server_para.MaxWoerkerNum, server_para.InitWorkerNum);

	return true;
}

bool server_close(struct Gthread_pool * pool)
{
	close_pool(pool);

	return true;
}
