#define _GNU_SOURCE
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<poll.h>
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
	struct pollfd *fds;
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

	fds = malloc(sizeof(struct pollfd) * (connfd + 1));
	if (!fds)
		return 0;

	for (i = 0; i <= connfd; i++) {
		fds[i].fd = i;
		fds[i].events = POLLIN | POLLOUT;
		fds[i].revents = 0;
	}

	while (1) {
		memset(buf, '\0', sizeof(buf));
		ret = poll(fds, connfd + 1, -1);
		if (ret < 0) {
			printf("poll failure\n");
			break;
		}

		for (i = 0; i <= connfd; i++) {
			if (fds[i].revents & POLLIN) {
				ret = recv(i, buf, sizeof(buf) - 1, 0);
				if (ret <= 0)
					break;

				printf("Get %d bytes of normal data: %s\n", ret, buf);
			}
		}
	}

	close(connfd);
	close(sock);
	free(fds);
	return 0;
}

