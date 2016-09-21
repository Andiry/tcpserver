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
#include<signal.h>
#include<sys/wait.h>
#include<sys/mman.h>
#include<sys/stat.h>

#define	USER_LIMIT	5
#define MAX_EVENTS	1024
#define BUFFER_SIZE	1024
#define	FD_LIMIT	65535
#define	PROCESS_LIMIT	65536

struct client_data
{
	struct sockaddr_in address;
	int connfd;
	pid_t pid;
	int pipefd[2];
};

static const char *shm_name = "/my_shm";
int sig_pipefd[2];
int epollfd;
int listenfd;
int shmfd;
char *share_mem;
struct client_data *users;
int *sub_process;
int user_count;
bool stop_child;

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
	send(sig_pipefd[1], (char *)&msg, 1, 0);
	errno = save_errno;
}

void addsig(int sig, void(*handler)(int), bool restart)
{
	struct sigaction sa;
	memset(&sa, '\0', sizeof(sa));
	sa.sa_handler = handler;
	if (restart)
		sa.sa_flags |= SA_RESTART;
	sigfillset(&sa.sa_mask);
	sigaction(sig, &sa, NULL);
}

void del_resource(void)
{
	close(sig_pipefd[0]);
	close(sig_pipefd[1]);
	close(listenfd);
	close(epollfd);
	shm_unlink(shm_name);
	free(users);
	free(sub_process);
}

void child_term_handler(int sig)
{
	stop_child = true;
}

int run_child(int idx, struct client_data *users, char *share_mem)
{
	struct epoll_event events[MAX_EVENTS];
	int child_epollfd = epoll_create(5);
	int connfd = users[idx].connfd;
	int pipefd = users[idx].pipefd[1];
	int number;
	int i;
	int ret;

	addfd(child_epollfd, connfd);
	addfd(child_epollfd, pipefd);
	addsig(SIGTERM, child_term_handler, false);

	while (!stop_child) {
		number = epoll_wait(child_epollfd, events, MAX_EVENTS, -1);

		for (i = 0; i < number; i++) {
			int sockfd = events[i].data.fd;
			if ((sockfd == connfd) && (events[i].events & EPOLLIN)) {
				memset(share_mem + idx * BUFFER_SIZE, '\0', BUFFER_SIZE);
				ret = recv(sockfd, share_mem + idx * BUFFER_SIZE, BUFFER_SIZE - 1, 0);
				if (ret == 0) {
					stop_child = true;
				} else if (ret < 0) {
					if (errno != EAGAIN)
						stop_child = true;
				} else {
					send(pipefd, (char *)&idx, sizeof(int), 0);
				}
			} else if ((sockfd == pipefd) && (events[i].events & EPOLLIN)) {
				int client = 0;
				ret = recv(sockfd, (char *)&client, sizeof(int), 0);
				if (ret == 0) {
					stop_child = true;
				} else if (ret < 0) {
					if (errno != EAGAIN)
						stop_child = true;
				} else {
					send(connfd, share_mem + client * BUFFER_SIZE, BUFFER_SIZE, 0);
				}
			}
		}
	}

	close(connfd);
	close(pipefd);
	close(child_epollfd);
	return 0;
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
	bool stop_server = false;
	bool terminate = false;

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

	users = malloc(sizeof(struct client_data) * (USER_LIMIT + 1));
	sub_process = malloc(sizeof(int) * PROCESS_LIMIT);
	for (i = 0; i < PROCESS_LIMIT; i++)
		sub_process[i] = -1;

	epollfd = epoll_create(MAX_EVENTS);
	if (epollfd <= 0)
		return 0;

	addfd(epollfd, sock);

	ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd);
	setnonblocking(sig_pipefd[1]);
	addfd(epollfd, sig_pipefd[0]);

	addsig(SIGCHLD, sig_handler, true);
	addsig(SIGTERM, sig_handler, true);
	addsig(SIGINT, sig_handler, true);
	addsig(SIGPIPE, SIG_IGN, true);

	shmfd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
	ret = ftruncate(shmfd, USER_LIMIT * BUFFER_SIZE);

	share_mem = mmap(NULL, USER_LIMIT * BUFFER_SIZE, PROT_READ | PROT_WRITE,
				MAP_SHARED, shmfd, 0);
	assert(share_mem != MAP_FAILED);
	close(shmfd);

	while (!stop_server) {
		int number;
		number = epoll_wait(epollfd, events, MAX_EVENTS, -1);
		if (number < 0) {
			printf("epoll failure\n");
			break;
		}

		for (i = 0; i < number; i++) {
			int sockfd = events[i].data.fd;
			if (sockfd == sock) {
				struct sockaddr_in client;
				pid_t pid;
				socklen_t client_addrlen = sizeof(client);
				int connfd = accept(sock, &client, &client_addrlen);
				if (connfd < 0)
					continue;

				if (user_count >= USER_LIMIT) {
					const char *info = "Too many users\n";
					printf("%s", info);
					send(connfd, info, strlen(info), 0);
					close(connfd);
					continue;
				}

				users[user_count].address = client;
				users[user_count].connfd = connfd;

				ret = socketpair(PF_UNIX, SOCK_STREAM, 0,
							users[user_count].pipefd);
				pid = fork();
				if (pid < 0) {
					close(connfd);
					continue;
				} else if (pid == 0) {
					close(epollfd);
					close(listenfd);
					close(users[user_count].pipefd[0]);
					close(sig_pipefd[0]);
					close(sig_pipefd[1]);
					run_child(user_count, users, share_mem);
					munmap(share_mem, USER_LIMIT * BUFFER_SIZE);
					exit(0);
				} else {
					close(connfd);
					close(users[user_count].pipefd[1]);
					addfd(epollfd, users[user_count].pipefd[0]);
					users[user_count].pid = pid;
					sub_process[pid] = user_count;
					user_count++;
				}
			} else if ((sockfd == sig_pipefd[0]) && (events[i].events & EPOLLIN)) {
				char signals[1024];
				ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
				if (ret == -1 || ret == 0)
					continue;
				else {
					pid_t pid;
					int stat;
					for (i = 0; i < ret; i++) {
						switch(signals[i]) {
						case SIGCHLD:
							while((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
								int del_user = sub_process[pid];
								sub_process[pid] = -1;
								if (del_user < 0 || del_user > USER_LIMIT)
									continue;
								epoll_ctl(epollfd, EPOLL_CTL_DEL,
										users[del_user].pipefd[0], 0);
								close(users[del_user].pipefd[0]);
								users[del_user] = users[--user_count];
								sub_process[users[del_user].pid] = del_user;
							}
							if (terminate && user_count == 0)
								stop_server = true;
							break;
						case SIGTERM:
						case SIGINT:
							printf("Kill all children now\n");
							if (user_count == 0) {
								stop_server = true;
								break;
							}
							for (i = 0; i < user_count; i++) {
								int pid = users[i].pid;
								kill(pid, SIGTERM);
							}
							terminate = true;
							break;
						default:
							break;
						}
					}
				}
			} else if (events[i].events & EPOLLIN){
				int child = 0;
				ret = recv(sockfd, (char *)&child, sizeof(child), 0);
				printf("Read data from child via pipe\n");
				if (ret == -1 || ret == 0)
					continue;
				else {
					for (i = 0; i < user_count; i++) {
						if (users[i].pipefd[0] != sockfd) {
							printf("Send data to children\n");
							send(users[i].pipefd[0], (char *)&child, sizeof(child), 0);
						}
					}
				}

			} else {
				printf("Something happened\n");
			}
		}
	}

	del_resource();
	return 0;
}

