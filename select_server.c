#define _GNU_SOURCE
#include<sys/types.h>
#include<sys/socket.h>
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
	char buf[1024];
	fd_set read_fds;
	fd_set exception_fds;
	int connfd;
	int sock;
	int reuse = 1;
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
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));
	ret = bind(sock, &address, sizeof(address));
	if (ret)
		return ret;

	ret = listen(sock, 5);
	if (ret)
		return ret;

	printf("Listening on port %d...\n", port);
	client_addrlen = sizeof(client);
	connfd = accept(sock, &client, &client_addrlen);
	if (connfd < 0)
		return connfd;

	FD_ZERO(&read_fds);
	FD_ZERO(&exception_fds);

	while (1) {
		memset(buf, '\0', sizeof(buf));
		FD_SET(connfd, &read_fds);
		FD_SET(connfd, &exception_fds);
		ret = select(connfd + 1, &read_fds, NULL, &exception_fds, NULL);
		if (ret < 0) {
			printf("selection failure\n");
			break;
		}

		if (FD_ISSET(connfd, &read_fds)) {
			ret = recv(connfd, buf, sizeof(buf) - 1, 0);
			if (ret <= 0)
				break;

			printf("Get %d bytes of normal data: %s\n", ret, buf);
		}

		if (FD_ISSET(connfd, &exception_fds)) {
			ret = recv(connfd, buf, sizeof(buf) - 1, MSG_OOB);
			if (ret <= 0)
				break;

			printf("Get %d bytes of normal data: %s\n", ret, buf);
		}
	}

	close(connfd);
	close(sock);
	return 0;
}

