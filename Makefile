both: diablo_cap reciever
diablo_cap: main.c
	gcc main.c -o diablo_cap -lpcap
reciever: reciever.cpp
	g++ reciever.cpp -o reciever
