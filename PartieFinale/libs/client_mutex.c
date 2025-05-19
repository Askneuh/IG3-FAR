#include <pthread.h>
#include "client_mutex.h"

pthread_mutex_t udp_socket_mutex;
pthread_mutex_t file_mutex;
pthread_mutex_t recv_mutex;
pthread_mutex_t opcode_mutex;

void init_mutexes() {
    pthread_mutex_init(&udp_socket_mutex, 0);
    pthread_mutex_init(&file_mutex, 0);
    pthread_mutex_init(&recv_mutex, 0);
    pthread_mutex_init(&opcode_mutex, 0);
}

void destroy_mutexes() {
    pthread_mutex_destroy(&udp_socket_mutex);
    pthread_mutex_destroy(&file_mutex);
    pthread_mutex_destroy(&recv_mutex);
    pthread_mutex_destroy(&opcode_mutex);
}

void init_opcode_mutex() {
    pthread_mutex_init(&opcode_mutex, 0);
}
void destroy_opcode_mutex() {
    pthread_mutex_destroy(&opcode_mutex);
}