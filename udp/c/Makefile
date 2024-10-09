ifeq (${CC}, )
$(error you need to set CC)
endif

all: server client

server: server.c
	${CC} $^ -o $@

client: client.c
	${CC} $^ -o $@

.PHONY: clean
clean:
	rm server client
