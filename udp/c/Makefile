ifeq (${CC}, )
$(error you need to set CC)
endif

CFLAGS := -Wall -Werror -O2

all: server client

server: server.c
	${CC} $^ -o $@ $(CFLAGS)

client: client.c
	${CC} $^ -o $@ $(CFLAGS)

.PHONY: clean
clean:
	rm server client
