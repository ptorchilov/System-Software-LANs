#include <stdio.h>
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

#define BUFFSIZE 1500

int sd;
pid_t pid;
struct sockaddr_in servaddr;
struct sockaddr_in from;

double tmin = 99999999.0;
double tmax = 0;
double tsum = 0;

int nsent = 0;
int nreceived = 0;

void pinger(void);
void output(char*, int, struct timeval*);
void catcher(int);
void tv_sub(struct timeval*, struct timeval*);
unsigned short in_cksum(unsigned short*, int);


// main fucntion of ping utility
int main(int argc, char *argv[])
{
	int size;
	int fromlen;
	int n;
	struct timeval tval;
	char recvbuf[BUFFSIZE];
	struct hostent *hp;
	struct sigaction act;
	struct itimerval timer;
	const int on = 1;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <hostname>\n", argv[0]);
		exit(-1);
	}

	pid = getpid();

	//handlers for SIGALRM and SIGINT signals
	memset(&act, 0, sizeof(act));

	//function for handlers - catcher()
	act.sa_handler = &catcher;
	sigaction(SIGALRM, &act, NULL);
	sigaction(SIGINT, &act, NULL);

	if ((hp = gethostbyname(argv[1])) == NULL) {
		herror("gethostbyname() failed");
		exit(-1);
	}

	if ((sd = socket(PF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
		perror("socket() failed");
		exit(-1);
	}

	//restore original rights
	setuid(getuid());

	//allow broadcast messages
	setsockopt(sd, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));

	//increase size of receive buffer
	size = 60 * 1024;
	setsockopt(sd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));

	//run timer for SIGALARM signal
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = 1;
	timer.it_interval.tv_sec = 1;
	timer.it_interval.tv_usec = 0;

	setitimer(ITIMER_REAL, &timer, NULL);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr = *((struct in_addr*) hp->h_addr);

	fromlen = sizeof(from);

	//loop for recieve packages
	while(1) {
		n = recvfrom(sd, recvbuf, sizeof(recvbuf), 0,
			(struct sockaddr*) &from, &fromlen);

		if (n < 0) {
			if (errno == EINTR) {
				continue;
			}

			perror("recvfrom() failed");
			continue;
		}

		//compute current time
		gettimeofday(&tval, NULL);

		//display ping info
		output(recvbuf, n, &tval);
	}

	return 0;
}

void output(char *ptr, int len, struct timeval *tvrecv) 
{
	int iplen;
	int icmplen;
	struct ip *ip;
	struct icmp *icmp;
	struct timeval *tvsend;
	double rtt;

	//begin of ip header
	ip = (struct ip*) ptr;

	//length of ip header
	iplen = ip->ip_hl << 2;

	//begin of icmp header
	icmp = (struct icmp*) (ptr + iplen);


	//length of icmp header
	if ((icmplen = len - iplen) < 8) {
		fprintf(stderr, "icmp (%d) < 8\n", icmplen);
	}

	if (icmp->icmp_type == ICMP_ECHOREPLY) {
		if (icmp->icmp_id != pid) {
			return;
		}

		tvsend = (struct timeval*) icmp->icmp_data;

		tv_sub(tvrecv, tvsend);

		//round-trip time (rtt) of package
		rtt = tvrecv->tv_sec * 1000.0 + tvrecv->tv_usec / 1000.0;

		nreceived++;

		tsum += rtt;
		if (rtt < tmin) {
			tmin = rtt;
		}
		if (rtt > tmax) {
			tmax = rtt;
		}

		printf("%d bytes from %s: icmp_seq=%u, ttl=%d, time=%.3f ms\n", 
			icmplen, inet_ntoa(from.sin_addr), icmp->icmp_seq, ip->ip_ttl, rtt);
	}
}

void pinger(void) 
{
	int icmplen;
	struct icmp *icmp;
	char sendbuf[BUFFSIZE];

	icmp = (struct icmp*) sendbuf;

	//fill all fields of icmp message
	icmp->icmp_type = ICMP_ECHO;
	icmp->icmp_code = 0;
	icmp->icmp_id = pid;
	icmp->icmp_seq = nsent++;
	gettimeofday((struct timeval*) icmp->icmp_data, NULL);

	icmplen = 8 + 56;

	//check sum of icmp-header and data
	icmp->icmp_cksum = 0;
	icmp->icmp_cksum = in_cksum((unsigned short *) icmp, icmplen);

	if (sendto(sd, sendbuf, icmplen, 0,
		(struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
		perror("sendto() failed");
		exit(-1);
	}
}

//subscription of two timeval stucts
void tv_sub(struct timeval *out, struct timeval *in) 
{
	if ((out->tv_usec -= in->tv_usec) < 0) {
		out->tv_sec--;
		out->tv_usec += 1000000;
	}

	out->tv_sec -= in->tv_sec;
}

//handler for SIGALRM and SIGINT
void catcher(int signum)
{
	if (signum == SIGALRM) {
		pinger();
		return;
	} else if (signum == SIGINT) {
		printf("\n--- %s ping statictics ---\n",
			inet_ntoa(servaddr.sin_addr));
		printf("%d packages transmitted, ", nsent);
		printf("%d packages received, ", nreceived);

		if (nsent) {
			if (nreceived > nsent) {
				printf("-- somebody's printing up packets!");
			} else {
				printf("%d%% packet loss", (int) (((nsent - nreceived) * 100) / nsent));
			}
		}

		printf("\n");

		if (nreceived) {
			printf("round-trip min/avg/max = %.3f/%.3f/%.3f ms\n", 
				tmin, tsum / nreceived, tmax);
		}

		fflush(stdout);
		exit(-1);
	}
}

//compute check sum
unsigned short in_cksum(unsigned short *addr, int len) 
{
	unsigned short result;
	unsigned int sum = 0;

	while (len > 1) {
		sum += *addr++;
		len -= 2;
	}

	if (len == 1) {
		sum += *(unsigned char *) addr;
	}

	//add carry-over
	sum = (sum >> 16) + (sum & 0xFFFF); 
	//again
	sum += (sum >> 16);
	//invert the result
	result = ~sum;

	return result;
}
