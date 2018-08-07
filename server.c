#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/sendfile.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_EVENT_NUMBER 1024
#define DEBUG

//线程的参数
typedef struct Arg_{
	int epfd;
	int readySock;
	struct sockaddr_in client;	
}Arg;

void Usage()
{
	printf("Usage: ./server [ip] [port] \n");
}

void* CreateRequest(void* ptr){
	Arg* arg = (Arg*)ptr;
	ProcessRequest((int)arg->epfd, (int)arg->readySock, (struct sockaddr_in*)&(arg->client));
	free(arg);
}

//从TCP缓冲区读取一行数据
int ReadLine(int sock, char* buf, int size){
	// \r \n  \r\n
	int index = 0;
	char c = 0;
#ifdef DEBUG
	printf("ReadLine\n");
#endif
	while(index < size -1 &&  c !='\n'){
		int ret = 0;
		if((ret = recv(sock, &c, 1, 0))> 0){
			if( c == '\r'){
				recv(sock ,&c, 1, MSG_PEEK);
				if(c == '\n'){
					recv(sock, &c, 1, 0);
				}
				else{
					c = '\n';
				}
			}
			buf[index++] = c;
		}	
		else if(ret == 0){
			return 0;
		}
	}
#ifdef DEBUG
	printf("ReadLine结束\n");
#endif
	buf[index] = 0;
	return index;
}

void ClearHeader(int sock){
	char buf[1024] = "";
	int ret = -1;
	while(strcmp(buf, "\n")!=0 && ret!=0){
		ret = ReadLine(sock, buf, sizeof(buf));
	}
#ifdef DEBUG
	printf("清除缓冲区\n");
#endif
}

//构造响应报文
int ResponseCli(int sock, char* path, int fileSize){
#ifdef DEBUG
	printf("开始构造响应报文\n");
#endif
	//path是资源地址，需要拷贝到sock里	
	const char* stat = "HTTP/1.0 200 OK\r\n";
	send(sock, stat, strlen(stat), 0);
	const char* type;
	//处理css和js类型响应
	if(strstr(path, ".css")){
		type = "Content-Type: text/css\r\n\r\n";
	}
	else if(strstr(path, ".js")){
		type = "Content-Type: application/x-javascript\r\n\r\n";
	}
	else if(strstr(path, ".jpg")){
		type = "Content-Type: image/jpeg\r\n\r\n";
	}
	else {
		type = "Content-Type: text/html\r\n\r\n";
	}

	send(sock, type, strlen(type), 0);
#ifdef DEBUG
	printf("向%d发送资源%s,大小%d\n", sock, path, fileSize);
#endif
	fflush(stdout);
	int fileFd = open(path, O_RDONLY);
	if(fileFd < 0){
		perror("open");
		return 404;
	}
	//内核中实现两文件描述符的拷贝，比write高效
	if(sendfile(sock, fileFd, 0, fileSize) < 0){
		perror("sendfile");
		close(fileFd);
		return 500;
	}	
	close(fileFd);
	return 200;
}

void ResponseErr(int sock, int stateCode){
	const char* stat;
	const char* html;
	switch(stateCode){
		case 400:stat = "HTTP/1.0 400 Bad Request\r\n";
				 html = "<HTML><title>400 Bad Request</title><h1>400 Bad Request</h1></HTML>";
				 break;
		case 403:stat = "HTTP/1.0 403 Forbidden\r\n";
				 html = "<HTML><title>403 Forbidden</title><h1>400 Forbidden</h1></HTML>";
				 break;
		case 404:stat = "HTTP/1.0 404 Not Found\r\n";
				 html = "<HTML><title>404 Not Found</title><h1>404 Not Found</h1></HTML>";
				 break;
		case 500:stat = "HTTP/1.0 500 Internal Server Error\r\n";
				 html = "<HTML><title>500 Internal Server Error</title><h1>500 Internal Server Error</h1></HTML>";
				 break;
		case 503:stat = "HTTP/1.0 503 Server Unavailable\r\n";
				 html = "<HTML><title>503 Server Unavailable</title><h1>503 Server Unavailable</h1></HTML>";
				 break;
		default:
				 break;
	}
	send(sock, stat, strlen(stat), 0);
	const char* type = "Content-Type: text/html\r\n\r\n";
	send(sock, type, strlen(type), 0);
	send(sock, html, strlen(html), 0);
#ifdef DEBUG
	printf("发送错误码%d\n", stateCode);
#endif
}

//父进程给子进程传参，环境变量在PCB不会被替换（shell例子）
//父进程需要拿到子进程的执行结果,重定向标准输入输出到管道,传入POST正文
int ExeCgi(int sock, char* method, char* path, char* queryString)
{
	//拿到get或者post的参数
	int ContentLength = -1;
	if(strcasecmp("GET", method) == 0){
		ClearHeader(sock);
	}	
	else{
		char buf[1024];
		do{
			ReadLine(sock, buf, sizeof(buf));
			if(strncasecmp(buf, "Content-Length: ", 16 ) == 0){
				ContentLength = atoi(&buf[16]);
			}
		}while(strcmp("\n", buf) != 0);
		if(ContentLength == -1){
			return 403;
		}
#ifdef DEBUG
		printf("正文长度%d\n", ContentLength);
#endif
	}
	int input[2];
	int output[2];
	if(pipe(input) < 0){
		perror("pipe");
		return 403;
	}
	if(pipe(output) < 0){
		perror("pipe");
		return 403;
	}

	pid_t pid;
	if((pid = fork()) < 0){
		perror("fork");
		return 403;
	}	
	if(pid == 0){
		//exec之前设置好参数
		dup2(input[0], 0);
		dup2(output[1], 1);
		close(input[1]);
		close(output[0]);
		char METHOD[20];
		char QUERY_STRING[1024];
		char CONTENT_LENGTH[1024];
		sprintf(METHOD,"METHOD=%s", method);
		putenv(METHOD);
		if(strcasecmp("GET", method) == 0){
			sprintf(QUERY_STRING,"QUERY_STRING=%s", queryString);
			putenv(QUERY_STRING);
		}
		else{
			sprintf(CONTENT_LENGTH, "CONTENT_LENGTH=%d", ContentLength);
			putenv(CONTENT_LENGTH);
		}
		execl(path, NULL);
		perror("exec");
		exit(1);
	}	
	//父进程需要拿到执行结果
	close(input[0]);
	close(output[1]);
	int i=0;	
	char c;
	for(; i< ContentLength; i++){
		recv(sock, &c, 1, 0);
		write(input[1], &c, 1);
	}

	char buf[1024];
	int readSize = 0;
	while(1){
		if((readSize = read(output[0], buf, sizeof(buf)-1)) > 0){
			send(sock, buf, readSize, 0);
		}
		else { 
			break;
		}
	}
	waitpid(pid, NULL, 0);
	close(input[1]);
	close(output[0]);
	return 200;
}

void ProcessRequest(int epfd, int sock, struct sockaddr_in* client){
	int stateCode = 200;
	char buf[1024] = {0};
	if(0 == ReadLine(sock, buf, sizeof(buf))){
#ifdef DEBUG
		printf("客户端关闭连接\n");
#endif
		struct epoll_event ev;
		epoll_ctl(epfd, EPOLL_CTL_DEL, sock, &ev);
		close(sock);
		return;
	}
#ifdef DEBUG
	printf("buf = %s", buf);
#endif
	//GET /index.html
	//获取方法
	char method[10] = {0};
	int i = 0;
	while(i < sizeof(method) && i < sizeof(buf) && buf[i]!= ' '){
		method[i] = buf[i];
		i++;
	}
	//去除多余空格
	while(i < strlen(buf) && buf[i] == ' '){
		i++;
	}
	//获取url
	char url[1024] = {0};
	int j = 0;	
	while(i<strlen(buf) && buf[i] != ' ' && buf[i] != '\n'){
		url[j++] = buf[i++];	
	}
#ifdef DEBUG
	printf("method = %s, url = %s\n", method, url);
#endif

	//支持get，和post方法
	int cgi = 0;
	char* queryString = NULL;
	if(strcasecmp(method, "GET") !=0 && strcasecmp(method, "POST") !=0 ){
		stateCode = 500;
		goto end;
	}
	if(strcasecmp(method, "GET") == 0){
		//判断get方法是否携带参数	
		i=0;
		while(i<strlen(url) && url[i]!= '?'){
			i++;
		}	
		if(i != strlen(url)){
			url[i] = '\0';
			queryString = url+i+1;
			cgi = 1;
		}
	}
	else if(strcasecmp(method, "POST") == 0){
		cgi = 1;	
	}
#ifdef DEBUG
	printf("cgi = %d, queryString:%s\n", cgi, queryString);
#endif
	//判断url访问资源，可能是 /   /可执行文件
	char path[1024];
	sprintf(path, "public%s", url);	
	//若请求的是 / ，返回目录下的index.html	
	if(path[strlen(path)-1] == '/'){
		sprintf(path, "%sindex.html", path);
	}
	//判断文件属性
	struct stat fileStat;
	if(stat(path, &fileStat)< 0){
#ifdef DEBUG
		printf("%s资源不存在\n", path);
#endif
		stateCode = 404;
		goto end;
	}
	if((fileStat.st_mode & S_IFMT) == S_IFDIR){
		sprintf(path, "%s/index.html", path);
	}
	//只要有可执行权限，则认为执行cgi
	if((fileStat.st_mode& S_IXUSR) || (fileStat.st_mode & S_IXGRP) 
			|| (fileStat.st_mode & S_IXOTH)){
		cgi = 1;
	}
	if(cgi){
#ifdef DEBUG
		printf("path:%s,进入cgi\n", path);
#endif
		if((stateCode = ExeCgi(sock, method, path, queryString)) == 200){
#ifdef DEBUG
			printf("cgi 正确运行完毕\n");
#endif
		}
	}
	else{
		//普通资源，直接返回用户请求的资源，此时tcp接收缓冲区还有数据，需要清除
		ClearHeader(sock);
		stateCode = ResponseCli(sock, path, fileStat.st_size);
	}
end:
	if(stateCode != 200){
		ClearHeader(sock);
		ResponseErr(sock, stateCode);
	}
#ifdef DEBUG
	printf("处理完毕，关闭%d连接\n\n", sock);
#endif
	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLONESHOT;
	ev.data.fd = sock;
	epoll_ctl(epfd, EPOLL_CTL_DEL, sock, &ev);
	close(sock);
}

//200 成功处理
//204 资源存在，内容为空
//206 断点续传，请求部分资源成功
//301 永久重定向,浏览器收到会自动再次发送post 改为get请求
//307 没有上述问题
//400 请求报文语法错误	
//403 资源存在，权限不够
//404 资源不存在
//500 服务端执行出错
//503 服务器超负荷，无法处理请求。

//makefile cgi: cd public/cgi; make clean;make; cd -
//mysql -h -u -p

int StartUp(char* argv[])
{
	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(atoi(argv[2]));
	server.sin_addr.s_addr = inet_addr(argv[1]);

	int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if(listen_sock < 0){
		perror("listen_socket");
		exit(2);
	}

	int opt = 1;
	setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	int ret = bind(listen_sock,(struct sockaddr*)&server, sizeof(server));
	if(ret < 0){
		perror("bind");
		exit(3);
	}

	//创建监听队列，最多有5个连接处于SYN?ESTA
	ret = listen(listen_sock, 20);
	if(ret < 0){
		perror("listen");
		exit(4);
	}
#ifdef DEBUG
	printf("listen...\n");
#endif

	return listen_sock;
}

int main(int argc, char* argv[]){
	if(argc <3){
		Usage();
		return 1;
	}
	signal(SIGPIPE, SIG_IGN);

	int listen_sock = StartUp(argv);
	int epfd = epoll_create(256);
	if(epfd < 0){
		printf("epoll_create error!\n");
		return 5;
	}
	struct epoll_event ev;
	struct epoll_event revs[MAX_EVENT_NUMBER];
	ev.events = EPOLLIN;
	ev.data.fd = listen_sock;
	epoll_ctl(epfd, EPOLL_CTL_ADD, listen_sock, &ev);

	for(;;){
		int timeout = -1;
		int num = epoll_wait(epfd, revs, sizeof(revs)/sizeof(revs[0]), timeout);
#ifdef DEBUG
		printf("就绪事件个数%d\n", num);
#endif
		switch(num){
			case -1:
				printf("epoll_wait error!\n");
				break;
			case 0:
				printf("timeout...\n");
				break;
			default :
				{
					int i = 0;
					struct epoll_event ev;
					for(i=0; i<num; i++){
						//取出就绪队列的事件
						struct sockaddr_in client;
						socklen_t len = sizeof(client);
						int readySock = revs[i].data.fd; 
						uint32_t events = revs[i].events;
#ifdef DEBUG
						printf("%d文件描述符就绪\n", readySock);
#endif

						if(readySock  == listen_sock && (events & EPOLLIN)){
							int newSock = accept(listen_sock, (struct sockaddr*)&client, &len);
							if(newSock <0){
								perror("accept");
								continue;
							}

							ev.events = EPOLLIN | EPOLLONESHOT;
							ev.data.fd = newSock;
							epoll_ctl(epfd, EPOLL_CTL_ADD, newSock, &ev);
#ifdef DEBUG
							printf("新客户端%d登录,设置其读关心\n", newSock);
#endif
						}
						else if(events & EPOLLIN) {
							pthread_t tid = 0;
							Arg* arg = (Arg*)malloc(sizeof(Arg));
							arg->epfd = epfd;
							arg->readySock = readySock;
							arg->client = client;
							pthread_create(&tid, NULL, CreateRequest, (void*)arg);
							if(pthread_detach(tid) < 0){
								perror("datach");
							}
#ifdef DEBUG
							printf("创建线程处理%d请求\n", readySock);
#endif
						}
						else if(events & EPOLLOUT){
							printf("未期待事件\n");
						}
						else{
							printf("未期待事件\n");
						}
					}
					break;
				}
		}
	}
}
