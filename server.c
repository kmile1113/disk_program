#include "my.h"

#define R 		1 //注册 register
#define L 		2 //登录 login
#define LIST	3 //列表
#define GET		4 //下载 
#define PUT		5 //上传
#define H  		6//查询历史
#define Q  		7//退出 quit
//注意 服务和客户端两端的请求类型宏定义 要一样

typedef struct 
{
	int type;//标志位 请求的类型
	char name[30];//用户名 
	char passwd[256];//用户密码
	char filename[50];//文件名 
	char filedata[100];//文件内容
	int len;//每次读取文件内容实际读取的块数
}MSG;

sqlite3* db = NULL;//数据库句柄定义为全局,任何函数有了句柄,就可以直接操作数据库
char sql[200] = { 0 };//用来保存数据库操作语句
char dirname[100] = { 0 };//用来保存设置的下载目录



void mySqlite3Exec(char* sql)
{
	int ret;
	char* errmsg = NULL;
	ret = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
	if(ret == 0)
	{
		printf("sqlite3_exec sucessful!!\n");
	}
	else
	{
		printf("sqlite3_exec failed:%s\n",errmsg);
	}
}
int mySqlite3GetTbale(char* sql)
{
	int i;
	char* errmsg = NULL;
	char** resultp = NULL;
	int row,column;
	int ret = sqlite3_get_table(db, sql, &resultp, &row, &column, &errmsg);
	if(ret == 0)
	{
		return row;//将数据库中的查询结果返回
		//row 代表查找的时候,满足条件信息的行数
		//row == 0 不存在这个信息,没找到 
		//row != 0 存在这个信息,查找到了
	}
	else
	{
		printf("sqlite3_get_table failed:%s\n",errmsg);
		exit(-1);//结束程序
	}
}


//处理客户端注册请求的函数
void doRegister(MSG* ps, int newsockfd)
{
	int ret;
	//总思想:打开数据库,取数据库查找用户名是否已经存在,给客户端一个应答
	sprintf(sql, "create table if not exists user_info (username vachar(20),passwd varchar(20));");
	ret = mySqlite3GetTbale(sql);
	sprintf(sql, "select * from user_info where username = '%s';",ps->name);
	ret = mySqlite3GetTbale(sql);
	if(ret == 0)//说明数据库中不存在该用户名,可以将用户名和秘密保存到数据库中
	{
		sprintf(sql,"insert into user_info values('%s','%s');",ps->name,ps->passwd);
		mySqlite3Exec(sql);
		//告诉客户一声,注册成功
		ps->type = 0;//表达成功
		sprintf(ps->passwd,"################恭喜你,注册成功!!!###############\n");
		send(newsockfd, ps, sizeof(MSG), 0);
	}
	else//说明用户名已经被注册了
	{
		ps->type = -1;//表达失败
		sprintf(ps->passwd,"################非常抱歉,注册失败!!!###############\n");
		send(newsockfd, ps, sizeof(MSG), 0);
	}
}

//处理客户端登录请求的函数
void doLogin(MSG* ps, int newsockfd)
{
	//1.去数据库里面比对,用户名和密码是否正确
	sprintf(sql, "select * from user_info where username = '%s' and passwd = '%s';",ps->name,ps->passwd);
	int ret = mySqlite3GetTbale(sql);
	if(ret == 0)//说明row的值是0,数据库中没有该用户名和密码
	{
		ps->type = -1;//-1表达登录失败!!
		sprintf(ps->passwd,"################非常抱歉,登录失败!!!###############\n");
		send(newsockfd, ps, sizeof(MSG), 0);
	}
	else//说明row的值不为0,数据库中有该用户名和密码,登录成功 
	{
		ps->type = 0;//0表达登录成功
		sprintf(ps->passwd,"################恭喜你,登录成功!!!###############\n");
		send(newsockfd, ps, sizeof(MSG), 0);
	}
	
}
//处理客户端获取文件列表请求函数 
void doGetFileList(MSG* ps, int newsockfd)
{
	//1.打开目录文件,循环读取每一个文件的名字,发送给客户端
	// /home/linux/aaaa
	struct dirent* ep = NULL;
	DIR* dp = opendir(dirname);
	if(dp == NULL)
	{
		perror("opendir failed");
		exit(-1);
	}
	while(1)
	{
		ep = readdir(dp);
		if(ep == NULL)//全部读取完了
		{
			break;
		}
		if(ep->d_name[0] == '.')//去掉隐藏文件
			continue;
		sprintf(ps->filename,"%s",ep->d_name);
		send(newsockfd, ps, sizeof(MSG), 0);
	}
	//额外在多发送一次,告诉客户端,已经全部发送完毕
	ps->type = -3;//-3代表全部发送完毕
	send(newsockfd, ps, sizeof(MSG), 0);
	closedir(dp);

}
void getSystemTime(char* p)
{
 time_t t;
 struct tm* tp = NULL;
 time(&t);//得到距今的秒数
 tp = localtime(&t);//转换为中文格式时间
 sprintf(p,"%d-%02d-%02d %02d:%02d:%02d",tp->tm_year+1900, tp->tm_mon+1, tp->tm_mday,tp->tm_hour, tp->tm_min, tp->tm_sec);
}
void doDownload(MSG* s, int newsockfd){
				char buf[100]={0};
				char file[100]={0};
				char systime[200] = { 0 };
				getSystemTime(systime);
				char state[10]="download";
				sprintf(sql, "insert into user_history values('%s','%s','%s','%s');",s->name,systime,state,s->filename);
				mySqlite3Exec(sql);
				sprintf(buf,"./%s",s->filename);
				sprintf(file,"%s/%s",dirname,s->filename);
				FILE* fpr=fopen(file,"r+");
				if(fpr==NULL){
					s->type=-1;
					return;
				}
				FILE* fpw=fopen(buf,"w+");
				if(fpw==NULL){
					perror("fopen failed");
					exit(-1);
				}
				while((s->len=fread(file,1,100,fpr))>0){
					fwrite(file,1,s->len,fpw);
					fflush(fpw);
				}
				fclose(fpw);
				fclose(fpr);
				s->type=0;
				
}
void upload(MSG* s, int newsockfd){
				char buf[100]={0};
				char file[100]={0};
				char systime[200] = { 0 };
				getSystemTime(systime);
				char state[10]="upload";
				sprintf(sql, "insert into user_history values('%s','%s','%s','%s');",s->name,systime,state,s->filename);
				mySqlite3Exec(sql);
				sprintf(buf,"./%s",s->filename);
				sprintf(file,"%s/%s",dirname,s->filename);
				FILE* fpr=fopen(buf,"r+");
				if(fpr==NULL){
					s->type=-1;
					return;
				}
				FILE* fpw=fopen(file,"w+");
				if(fpw==NULL){
					perror("fopne failed");
					exit(-1);
				}
				while((s->len=fread(s->filedata,1,100,fpr))>0){
					fwrite(s->filedata,1,s->len,fpw);
					fflush(fpw);
				}

				fclose(fpw);
				fclose(fpr);
				s->type=0;
				
}
void doHistoryRecord(MSG* p, int newsockfd)
{
 int i;
 char *errmsg = NULL,**resultp = NULL;
 int row, column;
 char sql[300] = { 0 };
 sprintf(sql,"select * from user_history where username = '%s';",p->name);

 int ret = sqlite3_get_table(db, sql, &resultp, &row, &column,&errmsg);
 if(ret != 0)
 {
 printf("sqlite3_get_table failed:%s\n",errmsg);
 exit(-1);
 }

 for(i = 0; i < (row+1)*column; i += column)
 {
 p->type = 0;
 //将每一行查询结果,拼接成一个字符串
 sprintf(p->filedata, "%s %s %s %s",resultp[i], resultp[i+1],resultp[i+2],resultp[i+3]);
 send(newsockfd, p, sizeof(*p), 0);

 }
 //循环结束之后,额外再多发送一次,告诉客户端,已经全部发送完毕
 p->type = -1;
 send(newsockfd, p, sizeof(*p), 0);
} 
//服务器器一直接收客户端请求的线程函数do_client
void* do_client(void* p)
{
	MSG s = { 0 };//用来保存接收的数据
	int ret;
	int newsockfd = *((int*)p);
	//一直接收客户端的请求
	while(1)
	{
		ret = recv(newsockfd, &s, sizeof(s), 0);
		if(ret > 0)
		{
			//打印就是为了调试程序
			printf("type:%d %s %s %s %s\n",s.type,s.name,s.passwd,s.filename,s.filedata);
			switch(s.type)
			{
			case R://处理注册请求
				doRegister(&s, newsockfd);
				break;
			case L://处理登录请求
				doLogin(&s, newsockfd);
				break;
			case LIST:
				doGetFileList(&s, newsockfd);
				break;
			case GET://处理下载请求
				doDownload(&s, newsockfd);
				send(newsockfd, &s, sizeof(s), 0);
				break;
			case PUT://处理上传请求
				upload(&s, newsockfd);
				send(newsockfd, &s, sizeof(s), 0);
				break;
			case H:
				doHistoryRecord(&s, newsockfd);
				break;
			case Q://处理上传请求
				doLogin(&s, newsockfd);
				break;
			}
		}
		else
		{
			printf("客户端newsockfd:%d断开!!\n",newsockfd);
			close(newsockfd);
			pthread_exit(NULL);
		}
	}
}

int main(int argc, const char *argv[])
{
	pthread_t id;
	int newsockfd;

	if(argc != 3)
	{
		printf("忘记传递参数了!! ./server port(6666) dirname(/home/linux/aaaaa)\n");
		exit(-1);
	}

	//从命令行参数得到下载的目录
	sprintf(dirname,"%s",argv[2]);

	//在mian函数中打开数据库,只打开一次就可以
	if(sqlite3_open("./my.db",&db) != 0)
	{
		perror("sqlite3 failed");
		exit(-1);
	}
	printf("sqlite3_open sucessful!!\n");

	sprintf(sql, "create table if not exists user_history (username vachar(20),time varchar(20),state varchar(20),filename varchar(20));");
	mySqlite3Exec(sql);
	//1.创建一个流式套接字
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd == -1)
	{
		perror("socket failed");
		exit(-1);
	}
	//2.绑定自己的IP地址和端口号
	struct sockaddr_in myaddr = { 0 };
	myaddr.sin_family = AF_INET;
	myaddr.sin_port = htons(atoi(argv[1]));
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	if(bind(sockfd, (struct sockaddr*)&myaddr, sizeof(myaddr)) == -1)
	{
		perror("bind failed");
		exit(-1);
	}
	printf("bind ok!!\n");
	//3.设置监听
	listen(sockfd, 5);
	//4.并发服务器
	while(1)
	{
		newsockfd = accept(sockfd, NULL, NULL);
		printf("client connect sucessful, newsockfd is %d\n",newsockfd);
		pthread_create(&id, NULL, do_client, &newsockfd);
	}
	//5.关闭套接字
	close(sockfd);
	return 0;
}
