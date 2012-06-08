#include <errno.h>
#include <stdio.h>
#include <pcap.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#define ETHER_ADDR_LEN 6
struct sniff_ethernet {
	u_char ether_dhost[ETHER_ADDR_LEN];
	u_char ether_shost[ETHER_ADDR_LEN];
	u_short ether_type;
};
struct sniff_ip {
	u_char ip_vhl;
	u_char ip_tos;
	u_short ip_len;
	u_short ip_id;
	u_short ip_off;
	u_char ip_ttl;
	u_char ip_p;
	u_short ip_sum;
	struct in_addr ip_src,ip_dst;
};
#define IP_HL(ip) (((ip)->ip_vhl) & 0x0f)
#define SIZE_ETHERNET 14
struct sniff_tcp {
	u_short th_sport;
	u_short th_dport;
	u_int32_t th_seq,th_ack;

	u_char th_offx2;
#define TH_OFF(th) (((th)->th_offx2 & 0xf0) >> 4)
	u_char th_flags;
#define TH_PUSH 0x08
	u_short th_win;
	u_short th_sum;
	u_short th_urp;
};
struct my_env {
	int socket;
	pcap_t *cap;
};
struct my_packet {
	int sport,dport;
	int size;
};
void handle_payload(const u_char *payload,u_int size,u_short sport,u_short dport,struct my_env *env) {
	if (dport == 1119) return;
	struct my_packet packet;
	packet.sport = sport;
	packet.dport = dport;
	packet.size = size;
	if (write(env->socket,&packet,sizeof(packet)) != sizeof(packet)) {
		printf("write 1 failed\n");
		pcap_close(env->cap);
	}
	if (write(env->socket,payload,size) != size) {
		printf("write 2 failed\n");
		pcap_close(env->cap);
	}
	printf("%u %u %u\n",sport,dport,size);
	u_char *x,*z,*i;
	unsigned int y = 1;
	z = payload;
	for (x = payload; x < (payload+size);x++,y++) {
		//printf("\n%x %x %x %d\n",x,z,i,y);
		printf("%02x ",*x);
		if (y < 5) {
			continue;
		}
		if ((y % 16) == 0) {
			for (i = z; i <=x; i++) {
				u_char t = *i;
				if (t < 0x20) printf("_");
				else if (t > 0x80) printf("_");
				else printf("%c",t);
			}
			z = x;
			printf("\n");
		}
		else if (y % 8 == 0) printf(" ");
	}
	printf("\n");
}
void handle_packet(struct my_env *env,const struct pcap_pkthdr *header,const u_char *packet) {
	const struct sniff_ethernet *ether = (struct sniff_ethernet*)packet;
	const struct sniff_ip *ip;
	const struct sniff_tcp *tcp;
	const u_char *payload;
	u_int size_ip,size_tcp;

 	ip = (struct sniff_ip*) (packet+SIZE_ETHERNET);
	size_ip = IP_HL(ip)*4;
	if (size_ip < 20) {
		printf("invalid ip header lenght %u\n",size_ip);
		return;
	}
	tcp = (struct sniff_tcp*) (packet+SIZE_ETHERNET+size_ip);
	size_tcp = TH_OFF(tcp)*4;
	if (size_tcp < 20) {
		printf("invalid tcp header size %u\n",size_tcp);
		return;
	}
	payload = (u_char *)(packet+SIZE_ETHERNET+size_ip+size_tcp);
	if ((tcp->th_flags & TH_PUSH) == 0) return;
	u_int size_payload = header->len - (SIZE_ETHERNET+size_ip+size_tcp);
	handle_payload(payload,size_payload,ntohs(tcp->th_sport),ntohs(tcp->th_dport),env);
}
int my_main(int sock) {
	char *dev = "lan0";
	char *errbuf[PCAP_ERRBUF_SIZE];
	struct bpf_program fp;
	struct my_env env;
	env.socket = sock;

	pcap_t *handle;
	handle = pcap_open_live(dev,2000,0,0,errbuf);
	env.cap = handle;
	if (handle == NULL) {
		printf("error 1: %s\n",errbuf);
		return 2;
	}
	if (pcap_compile(handle,&fp,"port 1119",0,0) == -1) {
		fprintf(stderr,"couldnt parse filter %s\n",pcap_geterr(handle));
		return 2;
	}
	if (pcap_setfilter(handle,&fp) == -1) {
		fprintf(stderr,"couldnt install filter %s\n",pcap_geterr(handle));
		return 2;
	}
	pcap_loop(handle,-1,handle_packet,&env);
	pcap_close(handle);
}
int main(int argc,char*argv[]) {
	struct sockaddr_in serv_addr,cli_addr;
	int sockfd,newsockfd;
	sockfd = socket(AF_INET,SOCK_STREAM,0);
	if (sockfd < 0) {
		printf("error making socket\n");
		return 3;
	}
	bzero((char*)&serv_addr,sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(20087);
	if (bind(sockfd,(struct sockaddr*)&serv_addr,sizeof(serv_addr)) < 0) {
		printf("error binding\n");
		return 3;
	}
	listen(sockfd,5);
	printf("waiting\n");
	socklen_t size = sizeof(cli_addr);
	while (newsockfd = accept(sockfd,(struct sockaddr*)&cli_addr,&size)) {
		printf("starting %d %d\n",newsockfd,errno);
		int ret = my_main(newsockfd);
		if (ret != 0) return ret;
	}
	return 0;
}
