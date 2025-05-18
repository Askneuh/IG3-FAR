CC = gcc
CFLAGS = -Wall -Wextra -pthread
DEPS = client_list.h command.h users.h

all: server client

server: server.o client_list.o command.o users.o
	$(CC) -o $@ $^ $(CFLAGS)

client: client.o
	$(CC) -o $@ $^ $(CFLAGS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f *.o server client
