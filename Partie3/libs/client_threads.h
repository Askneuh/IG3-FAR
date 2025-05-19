#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>


#ifndef CLIENT_THREADS_H
#define CLIENT_THREADS_H

struct ThreadContext {
    int dS;
    struct sockaddr_in aS;
    struct sockaddr_in aD;
    char username[MAX_USERNAME_LEN];
};

void* send_thread(void* arg);
void* recv_thread(void* arg);

#endif // CLIENT_THREADS_H