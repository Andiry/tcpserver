#define _GNU_SOURCE
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<pthread.h>
#include<sys/epoll.h>
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<errno.h>
#include<string.h>
#include<fcntl.h>
#include<stdbool.h>

#define MAX_EVENTS	1024
#define TCP_BUFFER_SIZE	512
#define UDP_BUFFER_SIZE	1024

int setnonblocking(int fd)
{
	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;

	fcntl(fd, F_SETFL, new_option);
	return old_option;
}

void addfd(int epollfd, int fd)
{
	struct epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLET;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	setnonblocking(fd);
}

int main(int argc, char *argv[])
{
	struct sockaddr_in address;
	int epollfd;
	int udpfd;
	struct epoll_event events[MAX_EVENTS];
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

	bzero(&address, sizeof(address));
	address.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &address.sin_addr);
	address.sin_port = htons(port);

	udpfd = socket(PF_INET, SOCK_DGRAM, 0);
	setsockopt(udpfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));
	ret = bind(udpfd, &address, sizeof(address));
	if (ret)
		return ret;

	epollfd = epoll_create(MAX_EVENTS);
	if (epollfd <= 0)
		return 0;

	addfd(epollfd, sock);
	addfd(epollfd, udpfd);

	while (1) {
		ret = epoll_wait(epollfd, events, MAX_EVENTS, -1);
		if (ret < 0) {
			printf("epoll failure\n");
			break;
		}

		for (i = 0; i < ret; i++) {
			int sockfd = events[i].data.fd;
			if (sockfd == sock) {
				struct sockaddr_in client;
				socklen_t client_addrlen = sizeof(client);
				int connfd = accept(sock, &client, &client_addrlen);
				addfd(epollfd, connfd);
			} else if (sockfd == udpfd) {
				char buf[UDP_BUFFER_SIZE];
				struct sockaddr_in client;
				socklen_t client_addrlen = sizeof(client);
				memset(buf, '\0', UDP_BUFFER_SIZE);

				ret = recvfrom(udpfd, buf, UDP_BUFFER_SIZE - 1, 0,
						&client, &client_addrlen);
				if (ret > 0)
					sendto(udpfd, buf, UDP_BUFFER_SIZE - 1, 0,
						&client, client_addrlen);
			} else if (events[i].events & EPOLLIN){
				char buf[TCP_BUFFER_SIZE];

				while (1) {
					memset(buf, '\0', TCP_BUFFER_SIZE);
					ret = recv(sockfd, buf, TCP_BUFFER_SIZE - 1, 0);
					if (ret < 0) {
						if (errno == EAGAIN || errno == EWOULDBLOCK)
							break;
						close(sockfd);
						break;
					} else if (ret == 0) {
						close(sockfd);
					} else {
						send(sockfd, buf, ret, 0);
					}
				}
			} else {
				printf("Something happened\n");
			}
		}
	}

	close(sock);
	return 0;
}

