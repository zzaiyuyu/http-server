#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define _BACKLOG_ 5

int main(int argc, char* argv[])
{

	char buf[1024];

	struct sockaddr_in server;
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	server.sin_family = AF_INET;
	server.sin_port = htons(atoi(argv[2]));
	server.sin_addr.s_addr = inet_addr(argv[1]);

	int ret = connect(sock, (struct sockaddr*)&server, sizeof(server));
	if(ret < 0){
		perror("connect");
		return 1;
	}
	printf("connect success\n");
	while(1){
		memset(buf, 0, sizeof(buf));
		printf("client:");
		fflush(stdout);
		int read_size = read(0, buf, sizeof(buf)-1);
		buf[read_size] = 0;
		write(sock, buf, read_size);
		printf("wait for server..\n");
		memset(buf, 0, sizeof(buf));
		if((read_size = read(sock, buf, sizeof(buf))) == 0){
			printf("server close!\n");
			fflush(stdout);
			break;
		}
		buf[read_size] = 0;
		printf("server:%s\n", buf);
	}
	close(sock);
}
