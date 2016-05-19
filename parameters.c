#include "config.h"

/*the data of the config for sever*/
struct server_conf server_para={
									"doc/cgi-bin/",//CGI Root
									"index.html",//Defaulr file
									"doc",//DocumenRoot
									"doc/config.conf",//ConfigFile
									54321,//Lsiten port
									10000,//max client
									5,//time out
									10,//init worker number
									2048,//max worker number
									};

/* the short cmd opt string */
static char * short_cmd_opt = "c:d:f:o:l:m:t:i:w:h";

/*the long cmd opt structure*/
static struct option long_cmd_opt[]={
									{"CGIRoot",        required_argument, NULL, 'c'},
									{"DefaultFile",    required_argument, NULL, 'd'},
									{"DocumentRoot",   required_argument, NULL, 'o'},
									{"ConfigFile",     required_argument, NULL, 'f'},
									{"ListenPort",     required_argument, NULL, 'l'},
									{"MaxClient",	   required_argument, NULL, 'm'},
									{"TimeOut",		   required_argument, NULL, 't'},
									{"InitWorkerNum",  required_argument, NULL, 'i'},
									{"MaxWoerkerNum",  required_argument, NULL,	'w'},
									{"help",		   no_argument, NULL, 'h'},
									};

/****************************************************************************
 *name:ConfReadLine
 *para1:the fd for the file (this fd should be opened before)
 *para2:the buffer pointer
 *para3: the buffer length
 *return: if succeed the number means how mucn bytes in the buffer, 0 means the endof the file
 *des:this function will get a line from the fd and store in buffer,the '\n' will be translated int *o '\0',so the buffer storing a string
 ***************************************************************************/
 static int ConfReadLine(int fd, char * buffer, int len);

/************************************************************************
 *name: GetParaFromFile
 *description: this function get the config information from the config file
 *para:the char pointer pointed to the string of name of the config file name
 *return: 1 for success, -1 for wrong
 *************************************************************************/
int GetParaFromFile(char * file)
{
#define LINELEN BUFSIZ
	int fd = -1, n = 0;
	char * name = NULL;
	char * value = NULL;
	char * pos = NULL;
	char line[LINELEN];

	if( (fd = open(file, O_RDONLY)) == -1 )//open the config file
	{
#if DEBUG == 1
		perror("Open confFile failed!");
#endif
		return -1;
	}

	while( (n = ConfReadLine(fd, line, LINELEN)) != 0)//the main loop to get a line from config file one by one
	{
		pos = line;
		if(*pos == '#')//'#'means this line is a comment line, so continue to get next line
			continue;
		while(isspace(*pos))//skip all the spaces before name
		  pos++;
		name = pos;
		
		while(!isspace(*pos) && *pos != '=')
		  pos++;
		*pos++ = '\0';//generate the name string in buffer
		
		while( isspace(*pos) || *pos == '=')//let pos point to value
			pos++;
		value = pos;
		while(!isspace(*pos) && *pos != '\0')
			pos++;	
		*pos = '\0';//generate the value string 

		/* set the value in the config structrue */
		if(strcmp(name, "CGIRoot") == 0)
			strcpy(server_para.CGIRoot, value);
		else if(strcmp(name, "DefaultFile") == 0)
			strcpy(server_para.DefaultFile, value);
		else if(strcmp(name, "DocumentRoot") == 0)
			strcpy(server_para.DocumentRoot, value);
		else if(strcmp(name, "ConfigFlie") == 0)
			strcpy(server_para.ConfigFile, value);
		else if(strcmp(name, "LsitenPort") == 0)
			server_para.ListenPort = atoi(value);
		else if(strcmp(name, "MaxClient") == 0)
			server_para.MaxClient = atoi(value);
		else if(strcmp(name, "TimeOut") == 0)
			server_para.TimeOut = atoi(value);
		else if(strcmp(name, "InitWorkerNum") == 0)
			server_para.InitWorkerNum = atoi(value);
		else if(strcmp(name, "MaxWorkerNum") == 0)
			server_para.MaxWoerkerNum = atoi(value);

	}
	return SUCCESS;
}

/****************************************************************************
 *name:ConfReadLine
 *para1:the fd for the file (this fd should be opened before)
 *para2:the buffer pointer
 *para3: the buffer length
 *return: if succeed the number means how mucn bytes in the buffer, 0 means the endof the file
 *des:this function will get a line from the fd and store in buffer,the '\n' will be translated int *o '\0',so the buffer storing a string
 ***************************************************************************/
 static int ConfReadLine(int fd, char * buffer, int len)
{	
	int n = 0, i = 0, begin = 0;
	memset(buffer, 0, len);
	for(;i < len; begin ? i++ : i)
	{
		n = read(fd, buffer+i, 1);
		if(n == 0)
		{
			*(buffer+i) = '\0';
			break;
		}
		else if( *(buffer+i) == '\n')
		{
			if( begin !=0 )
			{
				*(buffer+i) = '\0';
				break;
			}
			else
				continue;
		}
		else
			begin = 1;
	}

	return i;
}
								 


/************************************************************************
 *name: GetParaFromCmd
 *description: get the parameter from cmd lie
 *para1:argc(same with the main function)
 *para2:argv(same with the main function)
 *return: 1 for success, -1 for wrong
 *************************************************************************/
int GetParaFromCmd(int argc, char * argv[])
{

	if(argc < 2 || argv == NULL)
	{
		return SUCCESS;// there is no option in cmd line
	}

	int c = -1, value = -1;
	while( (c = getopt_long(argc, argv, short_cmd_opt, long_cmd_opt, NULL)) != -1)	
	{
		switch(c)
		{
			case 'c': printf("set CGIRoot: %s\n", optarg);
					  strcpy(server_para.CGIRoot, optarg);
					  break;
			case 'd': printf("set DefaultFile: %s\n", optarg);
					  strcpy(server_para.DefaultFile, optarg);
					  break;
			case 'o': printf("set DocumentRoot: %s\n", optarg);
					  strcpy(server_para.DocumentRoot, optarg);
					  break;
			case 'f': printf("set ConfigFile: %s\n", optarg);
					  strcpy(server_para.ConfigFile, optarg);
					  break;
			case 'l': value = atoi(optarg);
					  printf("set ListenPort: %d\n", value);
					  server_para.ListenPort = value;
					  break;
			case 'm': value = atoi(optarg);
					  printf("set MaxClient: %d\n", value);
					  server_para.MaxClient = value;
					  break;
			case 't': value = atoi(optarg);
					  printf("set TimeOut: %d\n", value);
					  server_para.TimeOut = value;
					  break;
			case 'i': value = atoi(optarg);
					  printf("set InitWorkerNum: %d\n", value);
					  server_para.InitWorkerNum = value;
					  break;
			case 'w': value = atoi(optarg);
					  printf("set MaxWoerkerNum: %d\n", value);
					  server_para.MaxWoerkerNum = value;
					  break;
			case 'h': printf("help test");
					  break;
			default: break;
		}
	}

	return SUCCESS;
}


/************************************************************************
 *name: DisplayConf
 *description: displau the sever config information
 *para:none
 *return: 1 for success, -1 for wrong
 *************************************************************************/
int DisplayConf()
{
	printf("http sever CGIRoot: %s\n", server_para.CGIRoot);
	printf("http sever DefaultFile: %s\n", server_para.DefaultFile);
	printf("http sever DocumentRoot: %s\n", server_para.DocumentRoot);
	printf("http sever ConfigFile: %s\n", server_para.ConfigFile);
	printf("http sever ListenPort: %d\n", server_para.ListenPort);
	printf("http sever MaxClient: %d\n", server_para.MaxClient);
	printf("http sever TimeOut: %d\n", server_para.TimeOut);
	printf("http sever InitWorkerNum: %d\n", server_para.InitWorkerNum);
	printf("http sever MaxWoerkerNum: %d\n", server_para.MaxWoerkerNum);

	return SUCCESS;
}
