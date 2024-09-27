#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sqlite3.h>
#include <signal.h>
#include <time.h>


#define N 32

#define R 1   // user - register
#define L 2   // user - login
#define Q 3   // user - query
#define H 4   // user - history

#define  DATABASE  "my.db"

// 定义通信双方的信息结构体
typedef struct {
	int type;
	char name[N];
	char data[256];
	int flags;
}MSG;

/*客户端消息处理*/
int do_client(int acceptfd, sqlite3 *db);
/*处理客户端注册请求*/
void do_register(int acceptfd, MSG *msg, sqlite3 *db);
/*处理客户端登录请求*/
int do_login(int acceptfd, MSG *msg, sqlite3 *db);
/*处理客户端查询请求*/
int do_query(int acceptfd, MSG *msg, sqlite3 *db);
int do_searchword(int acceptfd, MSG *msg, char word[]);
int get_date(char *date);
/*处理查看历史记录请求*/
int history_callback(void* arg,int f_num,char** f_value,char** f_name);
int do_history(int acceptfd, MSG *msg, sqlite3 *db);

/*******************************************/
/*********************主函数*****************/
/*******************************************/
int main(int argc, const char *argv[])
{

	int sockfd;
	struct sockaddr_in  serveraddr;
	int n;
	sqlite3 *db;
	int acceptfd;
	pid_t pid;
	//参数判断
	if(argc != 3)
	{
		printf("Usage:%s serverip  port.\n", argv[0]);
		return -1;
	}

	//打开数据库
	if(sqlite3_open(DATABASE, &db) != SQLITE_OK)
	{
		printf("%s\n", sqlite3_errmsg(db));
		return -1;
	}
	else
	{
		printf("open DATABASE success.\n");
	}
	//创建套接字
	if((sockfd = socket(AF_INET, SOCK_STREAM,0)) < 0)
	{
		perror("fail to socket.\n");
		return -1;
	}
	//设置通信结构体
	bzero(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(argv[1]);
	serveraddr.sin_port = htons(atoi(argv[2]));
	//绑定通信结构体
	if(bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
	{
		perror("fail to bind.\n");
		return -1;
	}

	// 将套接字设为监听模式
	if(listen(sockfd, 5) < 0)
	{
		printf("fail to listen.\n");
		return -1;
	}
	//处理僵尸进程
	struct sigaction act;
	act.sa_handler= SIG_IGN;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaction(SIGCHLD, &act, NULL);
	
	//多进程，处理客户端消息
	while(1)
	{
		if((acceptfd = accept(sockfd, NULL, NULL)) < 0)
		{
			perror("fail to accept");
			return -1;
		}

		//创建子进程来处理客户端的消息
		if((pid = fork()) < 0)
		{
			perror("fail to fork");
			return -1;
		}
		else if(pid == 0)
		{
			//处理客户端具体的消息
			close(sockfd);
			do_client(acceptfd, db);
		}
		else
		{
			close(acceptfd);
		}
	}

	return 0;
}


int do_client(int acceptfd, sqlite3 *db)
{
	MSG msg;
	bzero(&msg, sizeof(MSG));
	while(recv(acceptfd, &msg, sizeof(MSG), 0) > 0)
	{
		//if(msg.type == 0)
		//{
		//	continue;
		//}
		switch(msg.type)
		{
		case R:
			do_register(acceptfd, &msg, db);
			break;
		case L:
			do_login(acceptfd, &msg, db);
			break;
		case Q:
			do_query(acceptfd, &msg, db);
			break;
		case H:
			do_history(acceptfd, &msg, db);
			break;
		default:
			printf("Invalid data msg.\n");
		}

	}

	printf("client exit.\n");
	close(acceptfd);

	return 0;//隐式调用exit(0)
}

void do_register(int acceptfd, MSG *msg, sqlite3 *db)
{
	char * errmsg;
	char sql[128];

	sprintf(sql, "insert into usr values('%s', %s);", msg->name, msg->data);
	printf("%s\n", sql);

	if(sqlite3_exec(db,sql, NULL, NULL, &errmsg) != SQLITE_OK)
	{
		printf("%s\n", errmsg);
		strcpy(msg->data, "usr name already exist.");
	}
	else
	{
		printf("client  register ok!\n");
		strcpy(msg->data, "OK!");
	}

	if(send(acceptfd, msg, sizeof(MSG), 0) < 0)
	{
		perror("fail to send");
		return ;
	}

	return ;
}

int do_login(int acceptfd, MSG *msg , sqlite3 *db)
{
	char sql[128] = {};
	char *errmsg;
	int nrow;
	int ncloumn;
	char **resultp;
	//判断是否是root用户登录，设置flags
	if(strcmp(msg->name,"root") == 0)
	{
		msg->flags = 1;
	}
	else
	{
		msg->flags = 0;
	}

	sprintf(sql, "select * from usr where name = '%s' and pass = '%s';", msg->name, msg->data);
	printf("%s\n", sql);

	if(sqlite3_get_table(db, sql, &resultp, &nrow, &ncloumn, &errmsg)!= SQLITE_OK)
	{
		printf("%s\n", errmsg);
		return -1;
	}
	else
	{
		printf("get_table ok!\n");
	}

	// 查询成功
	if(nrow == 1)
	{
		strcpy(msg->data, "OK");
		send(acceptfd, msg, sizeof(MSG), 0);
		return 1;
	}

	if(nrow == 0) //密码或者用户名错误
	{
		strcpy(msg->data,"usr/passwd wrong.");
		send(acceptfd, msg, sizeof(MSG), 0);
	}

	return 0;
}

int do_searchword(int acceptfd, MSG *msg, char word[])
{
	FILE * fp;
	int len = 0;
	char temp[512] = {};
	int result;
	char *p;

	if((fp = fopen("dict.txt", "r")) == NULL)
	{
		perror("fail to fopen.\n");
		strcpy(msg->data, "Failed to open dict.txt");
		send(acceptfd, msg, sizeof(MSG), 0);
		return -1;
	}

	//打印出客户端要查询的单词
	len = strlen(word);
	printf("%s , len = %d\n", word, len);

	while(fgets(temp, 512, fp) != NULL)
	{
		result = strncmp(temp,word,len);

		if(result < 0)
		{
			continue;
		}
		if(result > 0 || ((result == 0) && (temp[len]!=' ')))
		{
			break;
		}

		p = temp + len;  
		while(*p == ' ')
		{
			p++;
		}

		// 跳跃过所有的空格
		strcpy(msg->data, p);

		// 注释拷贝完毕之后，应该关闭文件
		fclose(fp);
		printf("get word:%s\n",p);
		return 1;
	}

	fclose(fp);
	return -1;
}

int get_date(char *date)
{
	time_t t;
	struct tm *tp;

	time(&t);

	//进行时间格式转换
	tp = localtime(&t);

	sprintf(date, "%d-%d-%d %d:%d:%d", tp->tm_year + 1900, tp->tm_mon+1, tp->tm_mday, 
			tp->tm_hour, tp->tm_min , tp->tm_sec);
	printf("get date:%s\n", date);

	return 0;
}

int do_query(int acceptfd, MSG *msg , sqlite3 *db)
{
	char word[64];
	int found = 0;
	char date[128] = {};
	char sql[128] = {};
	char *errmsg;

	//拿出msg结构体中，要查询的单词
	int len = strlen(msg->data);
	msg->data[len] = '\0';
	strcpy(word, msg->data);

	found = do_searchword(acceptfd, msg, word);
	printf("查询一个单词完毕.\n");

	if(found == 1)
	{
		//获取时间
		get_date(date);

		sprintf(sql, "insert into record values('%s', '%s', '%s')", msg->name, date, word);

		if(sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
		{
			printf("%s\n", errmsg);
			return -1;
		}
		else
		{
			printf("Insert record done.\n");
		}

	}
	else
	{
		strcpy(msg->data, "Not found!");
	}

	// 将查询的结果，发送给客户端
	send(acceptfd, msg, sizeof(MSG), 0);
	return 1;
	
}

//查询记录，每找到一条记录调用一次
int history_callback(void* arg,int f_num,char** f_value,char** f_name)
{
	int acceptfd;
	MSG msg;

	acceptfd = *((int *)arg);

	sprintf(msg.data, "%-12s %s", f_value[1], f_value[2]);
	printf("%s\n",msg.data);
	send(acceptfd, &msg, sizeof(MSG), 0);
	return 0;
}


int do_history(int acceptfd, MSG *msg, sqlite3 *db)
{
	printf("into server do_history\n");
	char sql[128] = {};
	char *errmsg;
	//root用话查询整个记录表，普通用户只能查询自己的历史记录
	if(msg->flags == 0)
	{
		sprintf(sql, "select * from record where name = '%s'", msg->name);
	}
	else if(msg->flags == 1)
	{
		sprintf(sql, "select * from record;");
	}
	//查询数据库
	if(sqlite3_exec(db, sql, history_callback,(void *)&acceptfd, &errmsg)!= SQLITE_OK)
	{
		printf("%s\n", errmsg);
	}
	else
	{
		printf("Query record done.\n");
	}

	//所有的记录查询发送完毕之后，给客户端发出一个结束信息
	msg->data[0] = '\0';
	send(acceptfd, msg, sizeof(MSG), 0);

	return 0;
}



