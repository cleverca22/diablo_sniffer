#include "out/lib/rpc/connection.pb.h"

class myConnectionService : public ::bnet::protocol::connection::ConnectionService {
public:
	void Connect(google::protobuf::RpcController* controller,const ::bnet::protocol::connection::ConnectRequest* request,::bnet::protocol::connection::ConnectResponse* response,google::protobuf::Closure* done);
};
