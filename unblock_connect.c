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

#define BUFFER_SIZE	1024

int setnonblocking(int fd)
{
	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;

	fcntl(fd, F_SETFL, new_option);
	return old_option;
}

int unblock_connect(const char *ip, int port, int time)
{
	int sock;
	int fdopt;
	struct sockaddr_in address;
	char buf[BUFFER_SIZE];
	fd_set readfds;
	fd_set writefds;
	int reuse = 1;
	struct timeval timeout;
	int error;
	socklen_t length = sizeof(error);
	int ret;

	bzero(&address, sizeof(address));
	address.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &address.sin_addr);
	address.sin_port = htons(port);

	sock = socket(PF_INET, SOCK_STREAM, 0);
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));
	fdopt = setnonblocking(sock);

	ret = connect(sock, &address, sizeof(address));
	if (ret == 0) {
		printf("Connect with server immediately\n");
		fcntl(sock, F_SETFL, fdopt);
		return sock;
	} else if (errno != EINPROGRESS) {
		printf("Unblock connect not support\n");
		return -1;
	}

	FD_ZERO(&readfds);
	FD_SET(sock, &writefds);
	timeout.tv_sec = time;
	timeout.tv_usec = 0;

	ret = select(sock + 1, NULL, &writefds, NULL, &timeout);
	if (ret <= 0) {
		printf("Connection timeout\n");
		close(sock);
		return -1;
	}

	if (!FD_ISSET(sock, &writefds)) {
		printf("No events on sock\n");
		close(sock);
		return -1;
	}

	if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &length) < 0) {
		printf("get socket option failed\n");
		close(sock);
		return -1;
	}

	if (error != 0) {
		printf("No events on sock\n");
		close(sock);
		return -1;
	}

	printf("Connection ready after select with socket %d\n", sock);
	buf[0] = 'w';
	buf[1] = '\0';
	send(sock, buf, 10, 0); 
	fcntl(sock, F_SETFL, fdopt);
	return sock;
}

int main(int argc, char *argv[])
{
	int sock;
	char *ip;
	int port;

	if (argc <= 2) {
		printf("usage: %s ip_addr port\n", basename(argv[0]));
		return -EINVAL;
	}

	ip = argv[1];
	port = atoi(argv[2]);

	sock = unblock_connect(ip, port, 10);
	if (sock < 0)
		return 1;

	close(sock);
	return 0;
}

