#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>

#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H

struct UploadArgs {
    char filename[MAX_MSG_LEN];
    char username[MAX_USERNAME_LEN];
    int dS;
    struct sockaddr_in aS;
    struct sockaddr_in aD;
};

struct DownloadArgs {
    char filename[MAX_MSG_LEN];
    char username[MAX_USERNAME_LEN];
    int dS;
    struct sockaddr_in aS;
    struct sockaddr_in aD;
};

void upload_file(char *filename, char* username, int dS, struct sockaddr_in aS, struct sockaddr_in aD);
void download_file(char* filename, char* username, int dS, struct sockaddr_in aS, struct sockaddr_in aD);
void upload_file_wrapper(void* args);
void download_file_wrapper(void* args);

#endif // FILE_HANDLER_H