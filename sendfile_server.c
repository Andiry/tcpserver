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
	struct stat stat_buf;
	int connfd;
	int reuse = 1;
	int sock;
	int ret;
	int file_fd;
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

	file_fd = open("/mnt/ramdisk/test", O_RDONLY | O_CREAT, 0640);
	if (file_fd < 0)
		return file_fd;

	fstat(file_fd, &stat_buf);

	sendfile(connfd, file_fd, NULL, stat_buf.st_size);
	close(file_fd);
	close(connfd);

	close(sock);
	return 0;
}

