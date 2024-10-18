CC	:=	g++

CXXFLAGS	:=	-Wall -Werror -O2 -std=c++20

all: client

client.o:	../src/ftpClientHeadless.cpp
	$(CC) -c $< -o $@ $(CXXFLAGS)

client: client.o
	$(CC) $< -o $@ $(CXXFLAGS)

.PHONY: all clean

clean:
	rm client.o
