#ifndef MESSAGES_H
#define MESSAGES_H

#include <netinet/in.h>
#include "client_list.h"     // pour struct client, ClientNode
#include "common.h"       // pour struct msgBuffer
#define MAX_USERNAME_LEN 50
#define MAX_MSG_LEN 512
#define MAX_PASSWORD_LEN 50




// ✅ Envoie un message à un seul client
int sendMessageToOneClient(struct client c, struct msgBuffer* msg, int serverSocket);

// ✅ Envoie un message à un client à partir de son adresse
int sendMessageToClient(struct msgBuffer* msg, int serverSocket, struct sockaddr_in* adClient);

// (optionnel) Envoie à un tableau de clients si jamais tu veux faire une version tableau
void envoyerMessageAListe(ClientNode* clientList, struct msgBuffer* msg, int serverSocket, struct client* exception);


#endif
