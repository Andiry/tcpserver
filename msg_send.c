#define _GNU_SOURCE
#include<sys/types.h>
#include<sys/ipc.h>
#include<sys/socket.h>
#include<sys/msg.h>
#include<sys/wait.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<pthread.h>
#include<sys/epoll.h>
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<errno.h>
#include<signal.h>
#include<string.h>
#include<fcntl.h>
#include<stdbool.h>

struct message {
	long mtype;
	char mtext[20];
};

int main(void)
{
	pid_t pid;

	pid = fork();

	if (pid == 0) {
		struct message msgbuf_send;
		int msgid;
		msgid = msgget(357, 0666 | IPC_CREAT);

		msgbuf_send.mtype = 1;
		memcpy(msgbuf_send.mtext, "like\0", 15);
		msgsnd(msgid, &msgbuf_send, 10, IPC_NOWAIT);
		printf("Send message %s to %d\n", msgbuf_send.mtext, msgid);
		exit(0);
	} else {
		struct message msgbuf_recv;
		int msgid;
		int ret;

		wait(NULL);
		msgid = msgget(357, 0666);
		printf("Queue: %d\n", msgid);
		ret = msgrcv(msgid, &msgbuf_recv, 20, 1, IPC_NOWAIT);
		printf("%d: I got message %s from %d\n",
			ret, msgbuf_recv.mtext, msgid);
	}

	return 0;
}



