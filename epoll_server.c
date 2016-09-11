#define _GNU_SOURCE
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<sys/epoll.h>
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<errno.h>
#include<string.h>

#define MAX_EVENTS	8

int main(int argc, char *argv[])
{
	struct sockaddr_in address, client;
	socklen_t client_addrlen;
	char buf[1024];
	int epollfd;
	struct epoll_event fds[MAX_EVENTS];
	int connfd;
	int sock;
	int reuse = 1;
	int ret;
	int i;
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

	epollfd = epoll_create(MAX_EVENTS);
	if (epollfd <= 0)
		return 0;

	fds[0].events = EPOLLIN | EPOLLOUT;
	fds[0].data.fd = connfd;

	epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &fds[0]);
	while (1) {
		memset(buf, '\0', sizeof(buf));
		ret = epoll_wait(epollfd, fds, MAX_EVENTS, -1);
		if (ret < 0) {
			printf("epoll failure\n");
			break;
		}

		for (i = 0; i <= ret; i++) {
			if (fds[i].events & EPOLLIN) {
				ret = recv(fds[i].data.fd, buf, sizeof(buf) - 1, 0);
				if (ret <= 0)
					break;

				printf("Get %d bytes of normal data: %s\n", ret, buf);
			}
		}
	}

	close(connfd);
	close(sock);
	return 0;
}

