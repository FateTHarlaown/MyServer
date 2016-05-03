#include "server.h"
int main(int argc, char * argv[])
{
	Gthread_pool pool;
	server_init(&pool);
	request_handle(&pool);
	server_close(&pool);
	return 0;
}
