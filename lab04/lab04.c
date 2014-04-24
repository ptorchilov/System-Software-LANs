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
#include <pthread.h>
#include <vector>

#define GROUP "239.137.194.111"

using namespace std;

typedef enum { MESSAGE, COMMAND } message_t;

void sender(int);
void info_sender();
sockaddr_in select_interface(sockaddr_in* local_address);
int create_udp_socket();
void* receive_messages(void*);
void send_ip_address(int);


struct sockaddr_in multicast_address;
struct sockaddr_in local_address;
u_int16_t port;

int main(int argc, char* argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <port>\n", argv[0]);
		exit(-1);
	}

	port = atoi(argv[1]);

	select_interface(&local_address);
	inet_pton(AF_INET, GROUP, &multicast_address.sin_addr);	
	multicast_address.sin_port = htons(port);

	pthread_t receive;

	pthread_create(&receive, NULL, &receive_messages, NULL);

	int udp_descriptor = create_udp_socket();

	sender(udp_descriptor);

	return 0;
}

void sender(int udp_descriptor)
{
	char buf[1500];
	const char* list = "/list";

	printf("Multicast address: %s\n", inet_ntoa(multicast_address.sin_addr));
	printf("Enter your message: \n");

	__fpurge(stdin);

	while (1) {
		char *s = fgets(buf + 1, sizeof(buf) - 1, stdin);

		if (s == NULL && errno == EINTR) {
			continue;
		}

		message_t type;

		if (strncmp(s, list, 5) == 0) {
			type = COMMAND;
		} else {
			type = MESSAGE;
		}

		buf[0] = (char) type;

		if (sendto(udp_descriptor, buf, strlen(buf + 1) + 2, 0,
			(struct sockaddr*) &multicast_address, sizeof(multicast_address)) < 0) {
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

			char *addr = inet_ntoa(((sockaddr_in*) (ifa->ifa_addr))->sin_addr);
			printf("%d. Address: %s, ", i + 1, addr);
			addr = inet_ntoa(((sockaddr_in*) (ifa->ifa_netmask))->sin_addr);
			printf("mask: %s, ", addr);
			addr = inet_ntoa(((sockaddr_in*) (ifa->ifa_broadaddr))->sin_addr);
			printf("broadcast: %s \n", addr);
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

	struct sockaddr_in broadcast_address = *(sockaddr_in*) (ipv4_interfaces[num]->ifa_broadaddr);
	*local_address = *(sockaddr_in*) (ipv4_interfaces[num]->ifa_addr);


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
	
	unsigned int ttl = 1;
	setsockopt(descriptor, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
	
	int on = 1;
	setsockopt(descriptor, IPPROTO_IP, IP_MULTICAST_LOOP, &on, sizeof(on));

	return descriptor;
}

void* receive_messages(void*)
{
	int udp_descriptor = socket(PF_INET, SOCK_DGRAM, 0);

	if (udp_descriptor < 0) {
		perror("socket() failed.");
		exit(-1);
	}

	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));

	struct ip_mreq mreq;
	
	mreq.imr_multiaddr.s_addr = inet_addr(GROUP);
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	
	//in_addr_t broad_addr = inet_addr(inet_ntoa(broadcast_address.sin_addr));

	sin.sin_family = PF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = inet_addr(GROUP);

	setsockopt(udp_descriptor, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
	
	int on = 1;
	setsockopt(udp_descriptor, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	if (bind(udp_descriptor, (struct sockaddr* ) &sin, sizeof(sin)) < 0) {
		perror("bind() failed.");
		exit(-1);
	}

	struct sockaddr_in remote;
	socklen_t rlen = sizeof(remote);
	char buf[1500];

	int n;
	while (1) {
		n = recvfrom(udp_descriptor, buf, sizeof(buf), 0, (struct sockaddr*) &remote, &rlen);
		if (n < 0) {
			perror("Receive");
			continue;
		}
		message_t type = (message_t) buf[0];
		
		if (type == MESSAGE) {
			printf("\n[%s]: %s", inet_ntoa(remote.sin_addr), buf + 1);
	       		fflush(stdout);
		}	
		if (type == COMMAND){
			send_ip_address(udp_descriptor);
			continue;
		}
	}
}

void send_ip_address(int udp_descriptor)
{
	char buf[1500];
	message_t type = MESSAGE;
	buf[0] = (char) type;

	strncpy(buf + 1, inet_ntoa(local_address.sin_addr), sizeof(buf) - 2);

	if (sendto(udp_descriptor, buf, strlen(buf + 1) + 2, 0,
		(struct sockaddr*) &multicast_address, sizeof(multicast_address)) < 0) {
		perror("sendto() failed.");
		exit(-1);
	}
}

