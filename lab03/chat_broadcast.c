#include <ifaddrs.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <strings.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>
#include <string>
#include <map>
#include <pthread.h>

using namespace std;

typedef enum {TEXT, NOTIFICATION} message_t;

void sender();
void info_sender();
sockaddr_in select_interface(sockaddr_in* local_address);
int create_udp_socket()

struct sockaddr_in broadcast_address;
u_int16_t port;

int main(int argc, char* argv[]) 
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <port>\n", argv[0]);
		exit(-1);
	}

	port = atoi(argv[1]);
	sockaddr_in local_address;

	broadcast_address = select_interface(&local_address);
	broadcast_address.sin_port = htons(port);

	pthread_t receive;

	pthread_create(&receive, NULL, &receive_messages, NULL);

	int udp_descriptor = create_udp_socket();

	sender(udp_descriptor);

	return 0;
}

void sender(int udp_descriptor) 
{
	char buf[1500];
	char* list = "/list";

	printf("Broadcast address: %s\n", inte_ntoa(broadcast_address.sin_addr));
	printf("Enter your message: \n\n");

	__fpurge(stdin);

	while (1) {
		char *s = fgets(buf + 1, sizeof(buf) - 1, stdin);

		if (s == NULL && errno == EINTR) {
			continue;
		}

		if (strncmp(s, list, 5) == 0) {
			message_t type = NOTIFICATION;
		} else {
			message_t type = TEXT;
		}

		buf[0] = (char) type;

		if (sendto(udp_descriptor, buf, strlen(buf + 1) + 2, 0, 
			(struct sockaddr*) &broadcast_address, sizeof(broadcast_address)) < 0) {
			perror("sendto() failed.");
			exit(-1);
		}
	}
}

sockaddr_in select_interface(sockaddr_in* local_address) 
{
	struct ifaddrs *ifaddr, *ifa;

	if (getifaddrs(&ifaddr) == -1) {
		perror("getifaddrs() failed.");
		exit(-1);
	}

	vector<ifaddrs*> ipv4_interfaces;

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL) {
			continue;
		}

		int family = ifa->ifa_addr->sa_family;

		if (family == AF_INET && 
			inet_addr("127.0.0.1") != ((sockaddr_in*) (ifa->ifa_addr))->sin_addr.s_addr) {
			ipv4_interfaces.push_back(ifa);
		}
	}

	int interfces_count = ipv4_interfaces.size();
	int num;

	if (interfces_count > 1) {
		printf("\nChoose interface for broadcast:\n");

		for (int i = 0; i < interfces_count; ++i)
		{
			ifa = ipv4_interfaces[i];

			char *addr = inte_ntoa(((sockaddr_in*) (ifa->ifa_addr))->sin_addr);
			printf("%d. Address: %s, ", i + 1, addr);
			addr = inte_ntoa(((sockaddr_in*) (ifa->ifa_netmask))->sin_addr);
			printf("mask: %s, ", addr);
			addr = inte_ntoa(((sockaddr_in*) (ifa->ifa_broadcast))->sin_addr);
		}

		printf("\n");

		scanf("%d", &num);
		num--;

		if (num < 0 || num >= interfces_count) {
			exit(-1);
		}
	} else {
		num = 0;
	}

	struct sockaddr_in broadcast_address = *(sockaddr_in) (ipv4_interfaces[num]->ifa_broadcast);
	*local_address = *(sockaddr_in) (ipv4_interfaces[num]->ifa_addr);

	freeifaddrs(ifaddr);

	return broadcast_address;
}

int create_udp_socket() 
{
	int descriptor = socket(PF_INET, SOCK_DGRAM, 0);

	if (descriptor < 0) {
		perror("socket() failed.");
		exit(-1);
	}

	int on = 1;
	setsockopt(descriptor, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));

	return descriptor;
}

void receive_messages() 
{
	int udp_descriptor = socket(PF_INET, SOCK_DGRAM, 0);

	if (udp_descriptor < 0) {
		perror("socket() failed.");
		exit(-1);
	}

	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));

	sin.sin_family = PF_INET;
	sin.sin_port = port;
	sin.sin_addr.s_addr = broadcast_address;

	int on = 1;
	setsockopt(udp_descriptor. SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	if (bind(udp_descriptor, (struct sockaddr* ) &sin, sizeof(sin)) < 0) {
		perror("bind() failed.");
		exit(-1);
	}

	struct sockaddr_in remote;
	socklen_t rlen = sizeof(remote);
	char buf[1500];

	printf("Broadcast chat\n\n");

	while (1) {
		if (recvfrom(udp_descriptor, buf, sizeof(buf), 0, (struct sockaddr*) &remote, &rlen) < 0) {
			perror("Receive");
			continue;
		}

		message_t type = (message_t) buf[0];

		if (type == TEXT) {
			printf("\n[%s]: %s", inet_ntoa(remote.sin_addr), buf + 1);
		} else {

		}
	}
}