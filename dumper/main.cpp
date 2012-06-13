#include <windows.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <stdio.h>

void printError( TCHAR* msg );
void hexdump(unsigned char *buffer,DWORD addr,int size);
void show_count(struct posible_hit *hits);

int pidof (const char * findme) {
  HANDLE hProcessSnap;
  PROCESSENTRY32 pe32;
  hProcessSnap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
  if( hProcessSnap == INVALID_HANDLE_VALUE )
  {
    printError( TEXT("CreateToolhelp32Snapshot (of processes)") );
    return( 0 );
  }
  pe32.dwSize = sizeof( PROCESSENTRY32 );
  if( !Process32First( hProcessSnap, &pe32 ) )
  {
    printError( TEXT("Process32First") ); // show cause of failure
    CloseHandle( hProcessSnap );          // clean the snapshot object
    return( FALSE );
  }
  do
  {
    if (strcmp(findme,pe32.szExeFile) == 0) return pe32.th32ProcessID;
  } while( Process32Next( hProcessSnap, &pe32 ) );
  CloseHandle( hProcessSnap );
  return -1;
}
bool dump_page(HANDLE hndl,DWORD addr) {
	unsigned char buffer[512];
	memset(buffer,0,512);
	DWORD size = 0;
	if (!ReadProcessMemory(hndl,(void*)addr,buffer,512,&size)) {
		printf("error reading mem\n");
		return true;
	}
	hexdump(buffer,addr,size);
	return false;
}
struct posible_hit {
	DWORD address;
	struct posible_hit* next;
	bool valid;
};
struct posible_hit *initial_scan(int byte,HANDLE hndl) {
#define MB (1024*1024)
	DWORD size = 0;
	struct posible_hit *last_hit = NULL,*temp;
	unsigned char *buffer = new unsigned char[MB]; // one mb buffer
	for (int x = 0; x < (3); x++) { // loop over all 3 gig of ram
		memset(buffer,0,MB); // zero out buffer
		for (int y = 0; y < 2048; y++) { // for each page, read it over
			// if i tried to read the whole mb at once, it may fault out due to a single page
			if (!ReadProcessMemory(hndl,(void*)( (x*MB) + (y*512) ),buffer+(y*512),512,&size)) {
				// cant read this page, ignore
				//printf("cant read page %x\n",(x*MB) + (y*512));
			} else {
				//printf("loaded addr %x to %x\n",( (x*MB) + (y*512) ),(y*512));
			}
		}
		for (int z = 0; z < MB; z++) {
			if (buffer[z] == byte) { // hit
				//printf("\n hit! %x %x\n",z,buffer[z]);
				if (last_hit) {
					temp = last_hit;
					last_hit = new struct posible_hit;
					last_hit->next = temp;
				} else {
					last_hit = new struct posible_hit;
					last_hit->next = NULL;
				}
				last_hit->address = (x*MB) + z;
				last_hit->valid = true;
			}
		}
		printf("%x ",(x*MB));
		show_count(last_hit);
	}
	return last_hit;
}
struct posible_hit *refine_search(int byte,struct posible_hit *list,HANDLE hndl,int offset) {
	struct posible_hit *start = list,*current = list,*last = NULL;
	int count = 0;
	while (current) {
		if (current->valid == false) {
			current = current->next;
			continue;
		}
		if ((count % 10000) == 0) printf("%d %x %x\n",count,current,current->address);
		count++;
		unsigned char buffer[1];
		buffer[0] = 0;
		DWORD size = 0;
		if (!ReadProcessMemory(hndl,(void*) (current->address+offset),buffer,1,&size)) {
			current->valid = false;
		}
		if (buffer[0] != byte) { // miss, remove element
			current->valid = false;
		} else {
			last = current;
		}
		current = current->next;
	}
	printf("\n");
	return start;
}
void show_count(struct posible_hit *hits) {
	if (hits == NULL) return;
	int count = 1,bad=0;
	struct posible_hit *temp = hits;
	while (temp = temp->next) {
//		printf("#%u == %x\n",count,temp->address);
		if (temp->valid) count++;
		else bad++;
	}
	printf("count %u hits, %u bad\n",count,bad);
}
int main(int argc,char *argv[]) {
	int d3pid = pidof("Diablo III.exe");
	printf("D3\'s PID is %d\n",d3pid);
	if (d3pid < 1) {
		return 1;
	}
	HANDLE hProcess;
	hProcess = OpenProcess( PROCESS_ALL_ACCESS, FALSE, d3pid );
	if( hProcess == NULL ) {
		printError( TEXT("OpenProcess") );
		return 1;
	}
	printf("process is open\n");
	bool do_search = true;
	bool do_dump = false;
	if (do_search) {
		struct posible_hit *first_hit = NULL;
		printf("first byte? ");
		unsigned int byte;
		if (scanf("%x",&byte) != 1) {
			printf("error\n");
			return 1;
		}
		first_hit = initial_scan(byte,hProcess);
		show_count(first_hit);
		printf("first hit %x\n",first_hit->address);
		for (int x = 2; x < 65; x++) {
			printf("byte number %d? ",x);
			if (scanf("%x",&byte) != 1) {
				printf("error\n");
				return 1;
			}
			first_hit = refine_search(byte,first_hit,hProcess,x-1);
			show_count(first_hit);
			int count = 5;
			struct posible_hit *t = first_hit;
			do {
				if (t->valid && count--) dump_page(hProcess,t->address);
			} while (t = t->next);
		}
	}
	if (do_dump) {
		DWORD addr = 0x00800200;
	/*for (int x = 0; x < 500; x++) {
		DWORD *test = (DWORD*)buffer+x;
		printf("magic word is! %x at %x\n",*test,x);
	}*/
		for (DWORD x = 0; x < addr; x = x + 0x200) {
			dump_page(hProcess,x);
		}
	}
	CloseHandle(hProcess);
	return 0;
}
// buffer, a chunk of memory
// addr, the virtual addr the chunk original began at
void hexdump(unsigned char *buffer,DWORD addr,int size) {
	unsigned char *x,*z,*i;
	unsigned int y = 1;
	z = buffer;
	printf("%06x: ",addr);
	for (x = buffer; x < (buffer+size);x++,y++) {
		//printf("\n%x %x %x %d\n",x,z,i,y);
		printf("%02x ",*x);
		if (y < 5) {
			continue;
		}
		if ((y % 16) == 0) {
			for (i = z; i <=x; i++) {
				u_char t = *i;
				if (t < 0x20) printf("_");
				else if (t >= 0x80) printf("_");
				else printf("%c",t);
			}
			z = x+1;
			printf("\n%06x: ",addr+y);
		}
		else if (y % 8 == 0) printf(" ");
	}
	printf("\n");
}
void printError( TCHAR* msg ) {
  DWORD eNum;
  TCHAR sysMsg[256];
  TCHAR* p;

  eNum = GetLastError( );
  FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
         NULL, eNum,
         MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
         sysMsg, 256, NULL );

  // Trim the end of the line and terminate it with a null
  p = sysMsg;
  while( ( *p > 31 ) || ( *p == 9 ) )
    ++p;
  do { *p-- = 0; } while( ( p >= sysMsg ) &&
                          ( ( *p == '.' ) || ( *p < 33 ) ) );

  // Display the message
  _tprintf( TEXT("\n  WARNING: %s failed with error %d (%s)"), msg, eNum, sysMsg );
}
