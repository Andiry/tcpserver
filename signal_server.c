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
#include<signal.h>
#include<stdbool.h>

#define MAX_EVENTS	1024

static int pipefd[2];

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

void sig_handler(int sig)
{
	int save_errno = errno;
	int msg = sig;
	send(pipefd[1], (char *)&msg, 1, 0);
	errno = save_errno;
}

void addsig(int sig)
{
	struct sigaction sa;
	memset(&sa, '\0', sizeof(sa));
	sa.sa_handler = sig_handler;
	sa.sa_flags |= SA_RESTART;
	sigfillset(&sa.sa_mask);
	sigaction(sig, &sa, NULL);
}

int main(int argc, char *argv[])
{
	struct sockaddr_in address;
	int epollfd;
	struct epoll_event events[MAX_EVENTS];
	int sock;
	bool stop_server = false;
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

	addfd(epollfd, sock);

	ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
	setnonblocking(pipefd[1]);
	addfd(epollfd, pipefd[0]);

	addsig(SIGHUP);
	addsig(SIGCHLD);
	addsig(SIGTERM);
	addsig(SIGINT);

	while (!stop_server) {
		ret = epoll_wait(epollfd, events, MAX_EVENTS, -1);
		if (ret < 0 && errno != EINTR) {
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
			} else if (events[i].events & EPOLLIN && sockfd == pipefd[0]){
				char signals[1024];

				ret = recv(pipefd[0], signals, sizeof(signals), 0);
				if (ret == -1)
					continue;
				else if (ret == 0)
					continue;
				else {
					for (i = 0; i < ret; i++) {
						switch(signals[i]) {
							case SIGCHLD:
							case SIGHUP:
								continue;
							case SIGTERM:
							case SIGINT:
								printf("Received signal %d\n",
									signals[i]);
								stop_server = true;
							default:
								break;
						}
					}
				}
			} else {
				printf("Something happened\n");
			}
		}
	}

	close(sock);
	close(pipefd[1]);
	close(pipefd[0]);
	return 0;
}

