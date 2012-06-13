both: diablo_cap reciever
diablo_cap: main.c
	gcc main.c -o diablo_cap -lpcap
reciever: reciever.o connection.pb.o rpc.pb.o
	g++ reciever.o connection.pb.o rpc.pb.o -o reciever -lprotobuf
reciever.o: reciever.cpp
	g++ reciever.cpp -Iout/ -o reciever.o -c
connection.pb.o: out/lib/rpc/connection.pb.cc
	g++ out/lib/rpc/connection.pb.cc -o connection.pb.o -c -Iout/
rpc.pb.o: out/lib/rpc/rpc.pb.cc
	g++ out/lib/rpc/rpc.pb.cc -o $@ -c -Iout/
