#include "headers.h"
#include "rpc_host.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <google/protobuf/stubs/common.h>

void error(const char *msg)
{
    perror(msg);
    exit(0);
}
void myConnectionService::Connect(google::protobuf::RpcController* controller,const ::bnet::protocol::connection::ConnectRequest* request,::bnet::protocol::connection::ConnectResponse* response,google::protobuf::Closure* done) {
	printf("ran\n");
}
using namespace google::protobuf;
struct protobuf_packet {
	unsigned char type;
	unsigned int field;
	union {
		int number;
		char *string;
	} data;
	unsigned int bytes_used;
};
struct protobuf_packet parse_packet(unsigned char *buffer,int size) {
	struct protobuf_packet t;
	t.type = buffer[0] & 7;
	t.field = buffer[0] >> 3;
	unsigned char *pointer = buffer+1;
	unsigned char temp;
	printf("%02x field %u type %u\n",buffer[0],t.field,t.type);
	switch (t.type) {
		case 0: {
			temp = *pointer;
			int final_number = temp & 0x7f;
			t.bytes_used = 2;
			int offset = 0;
			while (temp & 0x80) {
				printf("more bytes remain\n");
				offset++;
				pointer++;
				temp = *pointer;
				final_number += temp << (7 * offset);
				printf("1: %u, 2: %u, 3: %u\n",final_number,temp,offset);
				t.bytes_used++;
			}
			t.data.number = final_number;
			printf("varint == %u\n",final_number);
			break;
			}
		case 1:
			printf("64bit number\n");
			t.bytes_used = 9;
			break;
		case 5:
			printf("32bit number\n");
			t.bytes_used = 5;
			break;
		default:
			printf("unknown type\n");
			exit(0);
	}
	return t;
}
int stream_size = 4;
unsigned char stream[] = { 0x16, 0x0b, 0x09, 0x1a };
int main(int argc, char *argv[])
{
    int sockfd, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

	unsigned char buffer[6000];
	unsigned char *header;
	struct my_packet packet;
    if (argc < 2) {
       fprintf(stderr,"usage %s hostname\n", argv[0]);
       exit(0);
    }
	//MyRpcServer server;
	Service *service = new myConnectionService;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");
	server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(20087);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");
	while (1) {
		n = read(sockfd,&packet,sizeof(packet));
		if (n == 0) break;
		if (n != sizeof(packet)) error("packet format error");
		n = read(sockfd,buffer,packet.size);
		if (n != packet.size) error("ERROR reading from socket");
		int field;
		int type;
		int header_size = ((buffer[0] & 0xff) << 8) + (buffer[1] & 0xff);
		if (header_size > 10000) continue; // too large
		if (header_size > 20) { // encrypted??
			printf("encrypted?\n");
			for (int x = 0; x < stream_size; x++) {
				buffer[x] = buffer[x] ^ stream[x];
			}
			header_size = ((buffer[0] & 0xff) << 8) + (buffer[1] & 0xff);
		}
		header = buffer+2;
		type = buffer[2] & 7;
		field = buffer[2] >> 3;
		printf("sport %u, %02x header_size:%u, field %u type %u\n",packet.sport,buffer[2],header_size,field,type);
		struct protobuf_packet p;
		int ServiceId = 0;
		int MethodId = 0;
		int Token = 0;
		int SizeTag = 0;
		do {
			p = parse_packet(header,header_size);
			header += p.bytes_used;
			header_size -= p.bytes_used;
			switch (p.field) {
				case 1:
					assert(p.type == 0);
					ServiceId = p.data.number;
					break;
				case 2:
					assert(p.type == 0);
					MethodId = p.data.number;
					break;
				case 3:
					assert(p.type == 0);
					Token = p.data.number;
					break;
				case 5:
					assert(p.type == 0);
					SizeTag = p.data.number;
					break;
				default:
					switch (p.type) {
						case 0:
							printf("field %u == %u\n",p.field,p.data.number);
							break;
						default:
							printf("field %u == ??\n",p.field);
					}
			}
		} while (header_size > 0);
		printf("%5u RPC call? service %u, method %u, token %u, data size: %u\n",packet.sport,ServiceId,MethodId,Token,SizeTag);
		assert(header_size == 0);
		if (SizeTag) {
			printf("ascii dump of packet payload\n");
			for (;header < (buffer + packet.size);header++) {
				if (*header < 0x20) printf("_");
				else if (*header > 0x80) printf("=");
				else printf("%c",*header);
			}
			printf("\n");
		}
		for (int i = 0; i < packet.size; i++) {
			u_char t;
			t = buffer[i];
			printf("%02x ",t);
		}
		printf("\n\n");
	}
	printf("done\n");
    close(sockfd);
    return 0;
}

