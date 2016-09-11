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
#include<poll.h>
#include<errno.h>
#include<string.h>

#define BUFFER_SIZE	64

int main(int argc, char *argv[])
{
	struct sockaddr_in address;
	struct pollfd pollfd[2];
	char readbuf[BUFFER_SIZE];
	int pipefd[2];
	int reuse = 1;
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
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));

	ret = connect(sock, &address, sizeof(address));
	if (ret < 0) {
		printf("Connection failed\n");
		close(sock);
		return 1;
	}

	pollfd[0].fd = 0;
	pollfd[0].events = POLLIN;
	pollfd[0].revents = 0;
	pollfd[1].fd = sock;
	pollfd[1].events = POLLIN | POLLRDHUP;
	pollfd[1].revents = 0;

	ret = pipe(pipefd);
	printf("%d %d\n", pipefd[0], pipefd[1]);

	while (1) {
		ret = poll(pollfd, 2, -1);
		if (ret < 0) {
			printf("Poll failure\n");
			break;
		}

		if (pollfd[1].revents & POLLRDHUP) {
			printf("Server close the connection\n");
			break;
		} else if (pollfd[1].revents & POLLIN) {
			memset(readbuf, '\0', BUFFER_SIZE);
			recv(pollfd[1].fd, readbuf, BUFFER_SIZE - 1, 0);
			printf("%s\n", readbuf);
		}

		if (pollfd[0].revents & POLLIN) {
			ret = splice(0, NULL, pipefd[1], NULL, 32768,
					SPLICE_F_MORE | SPLICE_F_MOVE);
			ret = splice(pipefd[0], NULL, sock, NULL, 32768,
					SPLICE_F_MORE | SPLICE_F_MOVE);
		}
	}

	close(sock);
	return 0;
}

