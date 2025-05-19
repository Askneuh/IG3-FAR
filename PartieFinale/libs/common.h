#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>


#ifndef COMMON_H
#define COMMON_H

#define MAX_USERNAME_LEN 50
#define MAX_MSG_LEN 512
#define MAX_PASSWORD_LEN 50

struct msgBuffer {
    char username[MAX_USERNAME_LEN];
    int opCode;
    char msg[MAX_MSG_LEN];
    int msgSize;
    int port;
    struct sockaddr_in adClient;
    char password[MAX_PASSWORD_LEN];
};



#endif // COMMON_H