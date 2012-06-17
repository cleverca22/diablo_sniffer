#include <stdlib.h>
#include <stdio.h>

int main(int argc,char *argv[]) {
	char sessionKey[64];
	for (int x = 0; x < 64; x++) sessionKey[x] = x;
	printf("%x\n",sessionKey);
	system("pause");
}
