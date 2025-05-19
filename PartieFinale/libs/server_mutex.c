#include <stdio.h>
#include <pthread.h>
#include "server_mutex.h"

pthread_mutex_t client_list_mutex;
pthread_mutex_t udp_socket_mutex;
pthread_mutex_t file_mutex;

void init_server_mutexes() {
    pthread_mutex_init(&client_list_mutex, 0);
    pthread_mutex_init(&udp_socket_mutex, 0);
    pthread_mutex_init(&file_mutex, 0);
}

void destroy_server_mutexes() {
    pthread_mutex_destroy(&client_list_mutex);
    pthread_mutex_destroy(&udp_socket_mutex);
    pthread_mutex_destroy(&file_mutex);
}