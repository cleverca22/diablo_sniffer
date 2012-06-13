CXX = ccache i586-mingw32msvc-g++ -g
LD = i586-mingw32msvc-g++ -g

OBJECTS = main.o

default: dumper.exe
dumper.exe: ${OBJECTS}
	${LD} ${OBJECTS} -o $@
