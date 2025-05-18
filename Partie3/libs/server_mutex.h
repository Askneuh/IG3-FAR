#include <stdio.h>
#include <pthread.h>
#ifndef SERVER_MUTEX_H
#define SERVER_MUTEX_H

extern pthread_mutex_t client_list_mutex;
extern pthread_mutex_t udp_socket_mutex;
extern pthread_mutex_t file_mutex;

void init_server_mutexes();
void destroy_server_mutexes();

#endif // SERVER_MUTEX_H