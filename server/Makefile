ifeq ($(CC),)
$(error No C compiler specified)
endif

CFLAGS		+=	-Wall -I include/

ifeq ($(DEBUG),y)
CFLAGS		+=	-g
else
CFLAGS		+=	-O2
endif

SRCS	+= src/main.c	src/listener.c
SRCS	+= src/logger.c	src/protocol.c
SRCS	+= src/ftp.c	src/filesystem.c
SRCS	+= src/pasv_channel.c
SRCS	+= src/port_channel.c

OBJS	:= $(SRCS:.c=.o)

TARGET	:= server

all: $(TARGET) clean

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)

.PHONY: all clean
