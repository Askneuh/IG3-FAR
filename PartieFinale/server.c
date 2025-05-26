#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "client_list.h"
#include <pthread.h>
#include "common.h"
#include "server_mutex.h"
#include "server_com.h"
#include "command.h"
#include "message.h"
#include <signal.h>
#include "salon.h"


#define MAX_USERNAME_LEN 50
#define MAX_MSG_LEN 512
#define MAX_USERS 100

static bool continueReceiving = true;

void shutdown_handler(int sig) {
    printf("\n🔴 Arrêt du serveur demandé...\n");
    sauvegarderSalons();
    continueReceiving = false;  // Arrêter la boucle
}


int main(int argc, char* argv[]) {

    signal(SIGTERM, shutdown_handler);
    signal(SIGINT, shutdown_handler);
    
    init_server_mutexes();

    chargerSalons();

    int dS = socket(AF_INET, SOCK_DGRAM, 0); 
    if (dS == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in adServeur;
    adServeur.sin_family = AF_INET; 
    adServeur.sin_addr.s_addr = INADDR_ANY; 
    adServeur.sin_port = htons((short)12345);
    if (bind(dS, (struct sockaddr*)&adServeur, sizeof(adServeur)) == -1) { 
        perror("bind");
        close(dS);
        exit(EXIT_FAILURE);
    }

    ClientNode* clientList = NULL;

    struct msgBuffer* msg = malloc(sizeof(struct msgBuffer)); 
    while(continueReceiving) {
        printf("En attente de message...\n");
        clientList = ReceiveMessage(dS, msg, clientList, adServeur); 
        pthread_mutex_lock(&client_list_mutex);
        printf("Liste des clients:\n");
        printClients(clientList);
        pthread_mutex_unlock(&client_list_mutex);
        printf("\n");
        if (strcmp(msg->msg, "quit") == 0) {
            printf("Arrêt du serveur demandé.\n");
            continueReceiving = false;
        }
    }
    destroy_server_mutexes();
    free(msg);
    freeClients(clientList);
    close(dS);
    return 0;
}