#define _GNU_SOURCE
#include<sys/socket.h>
#include<sys/sendfile.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<errno.h>
#include<string.h>

int main(int argc, char *argv[])
{
	struct sockaddr_in address, client;
	socklen_t client_addrlen;
	int pipefd[2];
	int connfd;
	int sock;
	int ret;
	char *ip;
	int port;

	if (argc <= 2) {
		printf("usage: %s ip_addr port\n", basename(argv[0]));
		return -EINVAL;
	}

	ip = argv[1];
	port = atoi(argv[2]);

	bzero(&address, sizeof(address));
	address.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &address.sin_addr);
	address.sin_port = htons(port);

	sock = socket(PF_INET, SOCK_STREAM, 0);
	ret = bind(sock, &address, sizeof(address));
	if (ret) {
		printf("bind to port %d fails\n", port);
		return ret;
	}

	ret = listen(sock, 5);
	if (ret)
		return ret;

	printf("Listening on port %d...\n", port);
	client_addrlen = sizeof(client);
	connfd = accept(sock, &client, &client_addrlen);
	if (connfd < 0)
		return connfd;

	ret = pipe(pipefd);
	printf("%d %d\n", pipefd[0], pipefd[1]);

	ret = splice(connfd, NULL, pipefd[1], NULL, 32768,
				SPLICE_F_MORE | SPLICE_F_MOVE);
	ret = splice(pipefd[0], NULL, connfd, NULL, 32768,
				SPLICE_F_MORE | SPLICE_F_MOVE);

	close(connfd);

	close(sock);
	return 0;
}

