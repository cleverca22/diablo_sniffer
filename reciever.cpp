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
struct protobuf_blob {
	int size;
	unsigned char *data;
};
struct protobuf_packet {
	unsigned char type;
	unsigned int field;
	union {
		int number;
		char *string;
		struct protobuf_blob blob;
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
				offset++;
				pointer++;
				temp = *pointer;
				final_number += temp << (7 * offset);
				//printf("1: %u, 2: %u, 3: %u\n",final_number,temp,offset);
				t.bytes_used++;
			}
			t.data.number = final_number;
			break;
			}
		case 1:
			printf("64bit number\n");
			t.bytes_used = 9;
			break;
		case 2: {
			int size2 = buffer[1];
			assert((size2 & 0x80) == 0); // doesnt handle varint right
			assert((size2+1) <= size);
			t.data.blob.data = new unsigned char[size2];
			t.data.blob.size = size2;
			memcpy(t.data.blob.data,buffer+1,size2);
			t.bytes_used = 2 + size2;
			break;
			}
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
class my_stream {
public:
	my_stream(){
		start = 0;
		end = 0;
	}
	void append(unsigned char *buf,int size) {
		memcpy(buffer + start,buf,size);
		end += size;
	}
	unsigned char peek(int offset) {
		assert(offset < size());
		return buffer[start+offset];
	}
	unsigned char read() {
		assert(size() > 0);
		unsigned char t = buffer[start];
		start++;
		check();
		return t;
	}
	int size() {
		return end - start;
	}
	void read(unsigned char *out,int size) {
		assert(size <= this->size());
		memcpy(out,buffer+start,size);
		start += size;
		check();
	}
	void reset() {
		printf("reset!!\n");
		start = 0;
		end = 0;
	}
private:
	void check() {
		if (size() == 0) {
			start = 0;
			end = 0;
		}
	}

	unsigned char buffer[65535];
	int start,end;
};
int entire_packet(my_stream stream) {
	int header_size = ((stream.peek(0) & 0xff) << 8) + (stream.peek(1) & 0xff);
	if (header_size > 10000) return 2;
	else if (header_size > 20) return 2;
	else if ((header_size+2) <= stream.size()) return 1;
	else return 0;
}
void hex_spew(unsigned char *buffer,int size) {
	for (int i = 0; i < size; i++) {
		u_char t;
		t = buffer[i];
		printf("%02x ",t);
	}
	printf("\n");
}
class rpc_state {
public:
	rpc_state() {
		bindToken = connectToken = 0;
	}
	void setConnectToken(int token) {
		connectToken = token;
	}
	void setBindToken(int token) {
		bindToken = token;
	}
	enum target { CLIENT,SERVER };
	void reply(enum target t,int token,int size,unsigned char *buffer) {
		if ((t == CLIENT) && (token == connectToken)) {
			handleConnectResponse(size,buffer);
		}
	}
	void handleConnectResponse(int size,unsigned char *buffer) {
		unsigned char *x = buffer;
		int size_left = size;
		do {
			struct protobuf_packet p;
			p = parse_packet(x,size_left);
			x += p.bytes_used;
			size_left -= p.bytes_used;
			switch (p.field) {
			case 1:
				assert(p.type == 2);
				printf("ConnectResponse.ProcessId == ");
				hex_spew(p.data.blob.data,p.data.blob.size);
				delete p.data.blob.data;
				break;
			case 2:
				assert(p.type == 2);
				printf("ConnectResponse.client_id == ");
				hex_spew(p.data.blob.data,p.data.blob.size);
				delete p.data.blob.data;
				break;
			case 5:
				assert(p.type == 2);
				printf("ConnectResponse.content_handle_array == ");
				hex_spew(p.data.blob.data,p.data.blob.size);
				delete p.data.blob.data;
				break;
			default:
				printf("field %u type %u\n",p.field,p.type);
			}
		} while (size_left > 0);
		assert(size_left == 0);
	}
private:
	int connectToken,bindToken;
};
class rpc_decoder {
public:
	void decode(my_stream stream,struct my_packet packet) {
		unsigned char *header,*buffer;;
		int header_size = ((stream.read() & 0xff) << 8) + (stream.read() & 0xff);
		//printf("sport %u, header_size:%u\n",packet.sport,header_size);
		buffer = header = new unsigned char[header_size];
		stream.read(header,header_size);
		int ServiceId = 0;
		int MethodId = 0;
		int Token = 0;
		int SizeTag = 0;
		do {
			struct protobuf_packet p;
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
		delete buffer;
		if (SizeTag) {
			buffer = new unsigned char[SizeTag];
			assert(SizeTag <= stream.size());
			stream.read(buffer,SizeTag);
			header = buffer;
			for (;header < (buffer + SizeTag);header++) {
				if (*header < 0x20) printf("_");
				else if (*header > 0x80) printf("=");
				else printf("%c",*header);
			}
			printf("\n");
			for (int i = 0; i < SizeTag; i++) {
				u_char t;
				t = buffer[i];
				printf("%02x ",t);
			}
			printf("\n");
			handle_rpc(ServiceId,MethodId,Token,SizeTag,buffer);
			delete buffer;
		} else {
			handle_rpc(ServiceId,MethodId,Token,0,NULL);
		}
	}
	virtual void handle_rpc(int service,int method,int token,int size,unsigned char *buffer) = 0;
protected:
	rpc_state *state;
};
class client_decoder : public rpc_decoder {
public:
	client_decoder(rpc_state *state) {
		this->state = state;
	}
private:
	void handle_rpc(int service,int method,int token,int size,unsigned char *buffer) {
		switch (service) {
		case 1:
			printf("CLIENT: AuthenticationClient Method %u, data %u, token %u\n",method,size,token);
			break;
		case 254:
			printf("CLIENT: reply for token %u, data %u\n",token,size);
			state->reply(rpc_state::CLIENT,token,size,buffer);
			break;
		default:
			printf("CLIENT: unknown service %u, method %u, data %u, token %u\n",service,method,size,token);
		}
	}
};
class server_decoder : public rpc_decoder {
public:
	server_decoder(rpc_state *state) {
		this->state = state;
	}
private:
	void handle_rpc(int service,int method,int token,int size,unsigned char *buffer) {
		switch (service) {
		case 0:
			switch (method) {
			case 1:
				printf("SERVER: ConnectionService::Connect, data %u, token %u\n",size,token);
				state->setConnectToken(token);
				break;
			case 2:
				printf("SERVER: ConnectionService::Bind, data %u, token %u\n",size,token);
				state->setBindToken(token);
				break;
			default:
				printf("SERVER: ConnectionService method %u, data %u, token %u\n",method,size,token);
			}
			break;
		case 1:
			switch (method) {
			case 1:
				printf("SERVER: AuthenticationServer::Logon, data %u, token %u\n",size,token);
				break;
			default:
				printf("SERVER: AuthenticationServer method %u\n",method);
			}
			break;
		default:
			printf("SERVER: unknown service %u, method %u, data %u, token %u\n",service,method,size,token);
		}
	}
};
int main(int argc, char *argv[]) {
	int sockfd, n;
	struct sockaddr_in serv_addr;
	struct hostent *server;
	
	unsigned char buffer[6000];
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
	bcopy((char *)server->h_addr,(char *)&serv_addr.sin_addr.s_addr,server->h_length);
	serv_addr.sin_port = htons(20087);
	if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) error("ERROR connecting");
	rpc_state state;
	my_stream server_stream; // server->client
	client_decoder client_rpc(&state); // client decoding above
	my_stream client_stream; // client->server
	server_decoder server_rpc(&state);
	while (1) {
		n = read(sockfd,&packet,sizeof(packet));
		if (n == 0) break;
		if (n != sizeof(packet)) error("packet format error");
		if (packet.syn) {
			if (packet.sport == 1119) server_stream.reset();
			else if (packet.dport == 1119) client_stream.reset();
		} else if (packet.push) {
			n = read(sockfd,buffer,packet.size);
			if (n != packet.size) error("ERROR reading from socket");
			if (packet.sport == 1119) server_stream.append(buffer,packet.size);
			else if (packet.dport == 1119) client_stream.append(buffer,packet.size);
			int rc;
			if (packet.sport == 1119) rc = entire_packet(server_stream);
			else if (packet.dport == 1119) rc = entire_packet(client_stream);
			if (rc == 1) {
				if (packet.sport == 1119) client_rpc.decode(server_stream,packet);
				else if (packet.dport == 1119) server_rpc.decode(client_stream,packet);
			} else if (rc == 2) continue;
			/*for (int i = 0; i < packet.size; i++) {
				u_char t;
				t = buffer[i];
				printf("%02x ",t);
			}*/
			printf("\n\n");
		}
	}
	printf("done\n");
    close(sockfd);
    return 0;
}

