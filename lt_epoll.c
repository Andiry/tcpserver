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
#include<fcntl.h>
#include<stdbool.h>

#define MAX_EVENTS	1024
#define BUFFER_SIZE	10

int setnonblocking(int fd)
{
	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;

	fcntl(fd, F_SETFL, new_option);
	return old_option;
}

void addfd(int epollfd, int fd, bool enable_et)
{
	struct epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN;
	if (enable_et)
		event.events |= EPOLLET;

	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	setnonblocking(fd);
}

void lt(struct epoll_event *events, int number, int epollfd, int listenfd)
{
	char buf[BUFFER_SIZE];
	struct sockaddr_in client;
	socklen_t client_addrlen;
	int connfd;
	int i;
	int sockfd;
	int ret;

	for (i = 0; i < number; i++) {
		sockfd = events[i].data.fd;
		if (sockfd == listenfd) {
			client_addrlen = sizeof(client);
			connfd = accept(listenfd, (struct sockaddr *)&client,
						&client_addrlen);
			addfd(epollfd, connfd, false);
		} else if (events[i].events & EPOLLIN) {
			printf("event trigger once\n");
			memset(buf, '\0', BUFFER_SIZE);
			ret = recv(sockfd, buf, BUFFER_SIZE, 0);
			if (ret <= 0) {
				close(sockfd);
				continue;
			}
			printf("Get %d bytes of content: %s\n", ret, buf);
		} else {
			printf("Something happened\n");
		}
	}
}

void et(struct epoll_event *events, int number, int epollfd, int listenfd)
{
	char buf[BUFFER_SIZE];
	struct sockaddr_in client;
	socklen_t client_addrlen;
	int connfd;
	int i;
	int sockfd;
	int ret;

	for (i = 0; i < number; i++) {
		sockfd = events[i].data.fd;
		if (sockfd == listenfd) {
			client_addrlen = sizeof(client);
			connfd = accept(listenfd, (struct sockaddr *)&client,
						&client_addrlen);
			addfd(epollfd, connfd, true);
		} else if (events[i].events & EPOLLIN) {
			printf("event trigger once\n");
			while (1) {
				memset(buf, '\0', BUFFER_SIZE);
				ret = recv(sockfd, buf, BUFFER_SIZE, 0);
				if (ret < 0) {
					if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
						printf("read later\n");
						break;
					}
					close(sockfd);
					break;
				} else if (ret == 0) {
					close(sockfd);
				} else {
					printf("Get %d bytes of content: %s\n", ret, buf);
				}
			}
		} else {
			printf("Something happened\n");
		}
	}
}

int main(int argc, char *argv[])
{
	struct sockaddr_in address;
	char buf[1024];
	int epollfd;
	struct epoll_event fds[MAX_EVENTS];
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

	epollfd = epoll_create(MAX_EVENTS);
	if (epollfd <= 0)
		return 0;

	addfd(epollfd, sock, true);

	while (1) {
		memset(buf, '\0', sizeof(buf));
		ret = epoll_wait(epollfd, fds, MAX_EVENTS, -1);
		if (ret < 0) {
			printf("epoll failure\n");
			break;
		}

		et(fds, ret, epollfd, sock);
	}

	close(sock);
	return 0;
}

