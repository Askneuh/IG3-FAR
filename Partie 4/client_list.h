#ifndef CLIENT_LIST_H
#define CLIENT_LIST_H

#include <netinet/in.h>
#include <stdbool.h>

#define MAX_USERNAME_LEN 50  // Ajoutez cette définition

struct client { 
    char username[MAX_USERNAME_LEN];
    struct sockaddr_in adClient;
    int port; 
    int dSClient; 
};

typedef struct ClientNode {
    struct client data;
    struct ClientNode* next;
} ClientNode;

// Fonctions disponibles pour d'autres fichiers
ClientNode* addClient(ClientNode* head, struct client newClient);
void printClients(ClientNode* head);
void freeClients(ClientNode* head);
bool clientAlreadyExists(ClientNode* head, struct client c);
int countClients(ClientNode* head);


#endif // CLIENT_LIST_H
