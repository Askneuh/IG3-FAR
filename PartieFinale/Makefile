CC = gcc
CFLAGS = -Wall -Wextra -pthread
LIBS_DIR = libs
INCLUDES = -I$(LIBS_DIR)
DEPS = $(LIBS_DIR)/client_list.h $(LIBS_DIR)/file_manager.h $(LIBS_DIR)/common.h $(LIBS_DIR)/client_mutex.h $(LIBS_DIR)/client_threads.h $(LIBS_DIR)/file_handler.h $(LIBS_DIR)/variables.h $(LIBS_DIR)/server_mutex.h $(LIBS_DIR)/server_com.h $(LIBS_DIR)/command.h $(LIBS_DIR)/message.h $(LIBS_DIR)/users.h $(LIBS_DIR)/salon.h

all: server client

server: $(LIBS_DIR)/server_mutex.o $(LIBS_DIR)/client_list.o $(LIBS_DIR)/server_com.o $(LIBS_DIR)/command.o $(LIBS_DIR)/message.o $(LIBS_DIR)/users.o $(LIBS_DIR)/file_manager.o $(LIBS_DIR)/salon.o server.o
	$(CC) -o $@ $^ $(CFLAGS)

client: client.o $(LIBS_DIR)/client_mutex.o $(LIBS_DIR)/client_threads.o $(LIBS_DIR)/file_handler.o $(LIBS_DIR)/variables.o
	$(CC) -o $@ $^ $(CFLAGS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(INCLUDES)

$(LIBS_DIR)/%.o: $(LIBS_DIR)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(INCLUDES)

clean:
	rm -f *.o $(LIBS_DIR)/*.o server client
