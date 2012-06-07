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
void handle_payload(const u_char *payload,u_int size,u_short sport,u_short dport) {
	printf("%u %u\n",sport,dport);
}
void handle_packet(u_char *args,const struct pcap_pkthdr *header,const u_char *packet) {
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
	handle_payload(payload,size_payload,ntohs(tcp->th_sport),ntohs(tcp->th_dport));
}
int main(int argc,char*argv[]) {
	char *dev = "lan0";
	char *errbuf[PCAP_ERRBUF_SIZE];
	struct bpf_program fp;

	pcap_t *handle;
	handle = pcap_open_live(dev,2000,0,0,errbuf);
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
	pcap_loop(handle,30,handle_packet,NULL);
	pcap_close(handle);
	return 0;
}
