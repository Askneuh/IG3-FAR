#ifndef MESSAGES_H
#define MESSAGES_H

#include <netinet/in.h>
#include "client_list.h"     // pour struct client, ClientNode

#define MAX_USERNAME_LEN 50
#define MAX_MSG_LEN 512

struct msgBuffer {
    char username[MAX_USERNAME_LEN];
    int opCode; 
    char msg[MAX_MSG_LEN];
    int msgSize;
    int port;
    struct sockaddr_in adClient;
    char password[MAX_PASSWORD_LEN];
};


// ✅ Envoie un message à un seul client
int sendMessageToOneClient(struct client c, struct msgBuffer* msg, int serverSocket);

// ✅ Envoie un message à tous les clients
int sendMessageToAllClients(ClientNode* clientList, struct msgBuffer* msg, int serverSocket);

// (optionnel) Envoie à un tableau de clients si jamais tu veux faire une version tableau
void envoyerMessageAListe(ClientNode* clientList, struct msgBuffer* msg, int serverSocket, struct client* exception);


#endif
