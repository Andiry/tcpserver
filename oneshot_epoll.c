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
#define BUFFER_SIZE	1024

struct fds {
	int epollfd;
	int sockfd;
};

int setnonblocking(int fd)
{
	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;

	fcntl(fd, F_SETFL, new_option);
	return old_option;
}

void addfd(int epollfd, int fd, bool oneshot)
{
	struct epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLET;
	if (oneshot)
		event.events |= EPOLLONESHOT;

	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	setnonblocking(fd);
}

void reset_oneshot(int epollfd, int fd)
{
	struct epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
	epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void* worker(void *arg)
{
	int sockfd = ((struct fds*)arg)->sockfd;
	int epollfd = ((struct fds*)arg)->epollfd;
	char buf[BUFFER_SIZE];
	int ret;

	printf("Start new thread to receive data on fd %d\n", sockfd);
	memset(buf, '\0', BUFFER_SIZE);

	while (1) {
		ret = recv(sockfd, buf, BUFFER_SIZE - 1, 0);
		if (ret == 0) {
			close(sockfd);
			printf("foreigner closed the connection\n");
			break;
		} else if (ret < 0) {
			if (errno == EAGAIN) {
				reset_oneshot(epollfd, sockfd);
				printf("Read later\n");
				break;
			}
		} else {
			printf("Get %d bytes of content: %s\n", ret, buf);
			sleep(5);
		}
	}
	printf("End thread receiving data on fd %d\n", sockfd);
	return NULL;
}

int main(int argc, char *argv[])
{
	struct sockaddr_in address;
	int epollfd;
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

	epollfd = epoll_create(MAX_EVENTS);
	if (epollfd <= 0)
		return 0;

	addfd(epollfd, sock, true);

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
				addfd(epollfd, connfd, true);
			} else if (events[i].events & EPOLLIN){
				pthread_t thread;
				struct fds fds_new;

				fds_new.epollfd = epollfd;
				fds_new.sockfd = sockfd;
				pthread_create(&thread, NULL, worker, &fds_new);
			} else {
				printf("Something happened\n");
			}
		}
	}

	close(sock);
	return 0;
}

