#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "exit.h"
#include "trinity.h"
#include "types.h"
#include "udp.h"
#include "utils.h"

#define MAXBUF 1024

// TODO: ipv6

struct sockaddr_in udpclient;

int socketfd;

static char buf[MAXBUF];

void sendudp(char *buffer, size_t len)
{
	int ret;

	ret = sendto(socketfd, buffer, len, 0, (struct sockaddr *) &udpclient, sizeof(udpclient));
	if (ret == -1) {
		fprintf(stderr, "sendto: %s\n", strerror(errno));
	}
}

static size_t readudp(void)
{
	int ret;
	socklen_t addrlen = 0;

	memset(buf, 0, MAXBUF);

	addrlen = sizeof(udpclient);
	ret = recvfrom(socketfd, buf, MAXBUF, 0, (struct sockaddr *) &udpclient, &addrlen);
	if (ret == -1)
		fprintf(stderr, "recvfrom: %s\n", strerror(errno));


	return ret;
}

/* simple 2-way handshake just to agree on protocol. */
static const char hello[] = "Trinity proto v" __stringify(TRINITY_UDP_VERSION);
static bool __handshake(void)
{
	char reply[] = "Trinity server v" __stringify(TRINITY_UDP_VERSION) ". Go ahead";

	if (strncmp(buf, hello, strlen(hello)) != 0) {
		printf("Incorrect message: %s\n", buf);
		return FALSE;
	}

	printf("Handshake request. sending reply (%ld bytes)\n", strlen(reply));

	sendudp(reply, strlen(reply));
	return TRUE;
}

static void handshake(void)
{
	int ret = -1;

retry:	while (ret != strlen(hello))
		ret = readudp();

	if (__handshake() == FALSE)
		goto retry;
}

static void decode_main_started(void)
{
	struct msg_mainstarted *mainmsg;

	mainmsg = (struct msg_mainstarted *) &buf;
	printf("Main started. pid:%d number of children: %d\n", mainmsg->pid, mainmsg->num_children);
}

static void decode_main_exiting(void)
{
	struct msg_mainexiting *mainmsg;

	mainmsg = (struct msg_mainexiting *) &buf;
	printf("Main exiting. pid:%d Reason: %s\n", mainmsg->pid, decode_exit(mainmsg->reason));
}

static void decode_child_spawned(void)
{
	struct msg_childspawned *childmsg;

	childmsg = (struct msg_childspawned *) &buf;
	printf("Child spawned. id:%d pid:%d\n", childmsg->pid, childmsg->childno);
}

static void decode_child_exited(void)
{
	struct msg_childexited *childmsg;

	childmsg = (struct msg_childexited *) &buf;
	printf("Child exited. id:%d pid:%d\n", childmsg->pid, childmsg->childno);
}

int main(__unused__ int argc, __unused__ char* argv[])
{
	int ret;
	struct sockaddr_in udpserver;

	socketfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (socketfd == -1) {
		fprintf(stderr, "Could not create a socket\n");
		goto out;
	}

	udpserver.sin_family = AF_INET;
	udpserver.sin_addr.s_addr = htonl(INADDR_ANY);
	udpserver.sin_port = htons(TRINITY_LOG_PORT);

	ret = bind(socketfd, (struct sockaddr *) &udpserver, sizeof(udpserver));
	if (ret != 0) {
		fprintf(stderr, "Could not bind to address!\n");
		goto closeout;
	}

	handshake();

	while (1) {
		int i;

		ret = readudp();

		// If something went wrong, just ignore and try again.
		if (ret <= 0)
			continue;

		/* We may see a new handshake appear at any time
		 * if a client dies without sending a 'main has exited' message.
		 * Just re-handshake for now. Later, we'll tear down any context etc.
		 */
		if (ret == strlen(hello)) {
			if (__handshake() == TRUE)
				continue;
		}

		switch (buf[0]) {
		case MAIN_STARTED:
			decode_main_started();
			break;

		case MAIN_EXITING:
			decode_main_exiting();
			break;

		case CHILD_SPAWNED:
			decode_child_spawned();
			break;

		case CHILD_EXITED:
			decode_child_exited();
			break;

		default:
			/* Unknown command (yet). Just dump as hex. */
			printf("rx %d bytes: ", ret);
			for (i = 0; i < ret; i++) {
				printf("%x ", (unsigned char) buf[i]);
			}
			printf("\n");
		}
	}

closeout:
	close(socketfd);
out:
	exit(EXIT_FAILURE);
}
