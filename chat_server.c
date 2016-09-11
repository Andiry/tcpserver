#define _GNU_SOURCE
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<poll.h>
#include<fcntl.h>
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<errno.h>
#include<string.h>

#define	USER_LIMIT	5
#define	BUFFER_SIZE	64
#define	FD_LIMIT	65535

struct client_data {
	struct sockaddr_in address;
	char *write_buf;
	char buf[BUFFER_SIZE];
};

int setnonblocking(int fd)
{
	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;

	fcntl(fd, F_SETFL, new_option);
	return old_option;
}

int main(int argc, char *argv[])
{
	struct sockaddr_in address;
	struct client_data *users;
	struct pollfd fds[USER_LIMIT + 1];
	int user_counter = 0;
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

	users = malloc(sizeof(struct client_data) * FD_LIMIT);
	if (!users)
		return 1;

	for (i = 1; i <= USER_LIMIT; i++) {
		fds[i].fd = -1;
		fds[i].events = 0;
	}

	fds[0].fd = sock;
	fds[0].events = POLLIN | POLLERR;
	fds[0].revents = 0;

	while (1) {
		ret = poll(fds, user_counter + 1, -1);
		if (ret < 0) {
			printf("Poll failure\n");
			break;
		}

		for (i = 0; i < user_counter + 1; i++) {
			if (fds[i].fd == sock && fds[i].revents & POLLIN) {
				struct sockaddr_in client;
				socklen_t client_addrlen = sizeof(client);

				connfd = accept(sock, &client, &client_addrlen);
				if (connfd < 0) {
					printf("errno %d\n", errno);
					continue;
				}

				if (user_counter >= USER_LIMIT) {
					const char *info = "Too many users\n";
					printf("%s", info);
					send(connfd, info, strlen(info), 0);
					close(connfd);
					continue;
				}

				user_counter++;
				users[connfd].address = client;
				setnonblocking(connfd);
				fds[user_counter].fd = connfd;
				fds[user_counter].events = POLLIN | POLLRDHUP | POLLERR | POLLOUT;
				fds[user_counter].revents = 0;
				printf("A new user connects, now %d users\n", user_counter);
			} else if (fds[i].revents & POLLERR) {
				char errors[100];
				socklen_t length = sizeof(errors);
				printf("Get an error from %d\n", fds[i].fd);
				memset(errors, '\0', 100);
				if (getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR,
								&errors, &length) < 0) {
					printf("Get socket option failure\n");
				}
				continue;
			} else if (fds[i].revents & POLLRDHUP) {
				users[fds[i].fd] = users[fds[user_counter].fd];
				close(fds[i].fd);
				fds[i] = fds[user_counter];
				i--;
				user_counter--;
				printf("A client left\n");
			} else if (fds[i].revents & POLLIN) {
				connfd = fds[i].fd;
				memset(users[connfd].buf, '\0', BUFFER_SIZE);
				ret = recv(connfd, users[connfd].buf, BUFFER_SIZE - 1, 0);
				printf("Get %d bytes of client data %s from %d\b", ret,
						users[connfd].buf, connfd);
				if (ret < 0) {
					if (errno != EAGAIN) {
						close(fds[i].fd);
						users[fds[i].fd] = users[fds[user_counter].fd];
						fds[i] = fds[user_counter];
						i--;
						user_counter--;
					}
				} else if (ret > 0) {
					int j;
					for (j = 1; j <= user_counter; j++) {
						if (fds[j].fd == connfd)
							continue;
//						fds[j].events &= ~POLLIN;
//						fds[j].events |= POLLOUT;
						users[fds[j].fd].write_buf = users[connfd].buf;
					}
				}
			} else if (fds[i].revents & POLLOUT) {
				connfd = fds[i].fd;
				if (!users[connfd].write_buf)
					continue;
				ret = send(connfd, users[connfd].write_buf,
						strlen(users[connfd].write_buf), 0);
				users[connfd].write_buf = NULL;
//				fds[i].events &= ~POLLOUT;
//				fds[i].events |= POLLIN;
			}
		}
	}

	close(sock);
	free(users);
	return 0;
}

