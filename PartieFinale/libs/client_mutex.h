#ifndef CLIENT_MUTEX_H
#define CLIENT_MUTEX_H

#include <pthread.h>

extern pthread_mutex_t udp_socket_mutex;
extern pthread_mutex_t file_mutex;
extern pthread_mutex_t recv_mutex;
extern pthread_mutex_t opcode_mutex;

void init_mutexes();
void destroy_mutexes();
void init_opcode_mutex();
void destroy_opcode_mutex();
#endif // MUTEX_H