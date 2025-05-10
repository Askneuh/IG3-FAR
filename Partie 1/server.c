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

#define MAX_USERNAME_LEN 50
#define MAX_MSG_LEN 512

//Choix effectués : échange UDP pour les messages et TCP pour  l'echange de fichiers.

struct msgBuffer {
    char username[MAX_USERNAME_LEN];
    int opCode; 
    char msg[MAX_MSG_LEN];
    int msgSize;
    int port;
    struct sockaddr_in adClient;
};

int sendMessageToAllClients(ClientNode* clientList, struct msgBuffer* msg, int serverSocket) {
    ClientNode* current = clientList;
    int successCount = 0;
    int errorCount = 0;
    
    while (current != NULL) {
        // Ne pas envoyer au client qui a émis le message
        if (strcmp(current->data.username, msg->username) != 0) { 
            socklen_t addrLen = sizeof(struct sockaddr_in);
            int result = sendto(serverSocket, msg, sizeof(struct msgBuffer), 0, 
                               (struct sockaddr*)&current->data.adClient, addrLen);
            
            if (result == -1) {
                perror("Erreur lors de l'envoi du message");
                errorCount++;
            } else {
                successCount++;
                printf("Message transmis à %s\n", current->data.username);
            }
        }
        current = current->next;
    }
    
    printf("Message diffusé à %d clients (%d erreurs)\n", successCount, errorCount);
    return successCount;
}


ClientNode* ReceiveMessage(int dS, struct msgBuffer* msg, ClientNode* clientList) {

    struct sockaddr_in adrExp;
    socklen_t adrExpLen = sizeof(adrExp);
    if (recvfrom(dS, msg, sizeof(struct msgBuffer), 0, (struct sockaddr*)&adrExp, &adrExpLen) == -1) { // On reçoit le message
        perror("recvfrom");
        close(dS);
        exit(EXIT_FAILURE);
    }
    printf("Received message from %s\n", msg->username); 
    printf("Message reçu : %s\n", msg->msg); 

    struct client c;
    c.adClient = msg->adClient;
    c.port = msg->port;
    c.dSClient = dS;
    strcpy(c.username, msg->username); 
    c.adClient.sin_port = htons(msg->port); 
    
    if (!clientAlreadyExists(clientList, c)) { 
        clientList = addClient(clientList, c); 
        printf("Nouveau client ajouté: %s\n", c.username);
    }
    if (msg->opCode == 1) {  
        sendMessageToAllClients(clientList, msg, dS); 
    }
    return clientList; 
}


int main(int argc, char* argv[]) {
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
    
    while(1) {
        printf("En attente de message...\n");
        clientList = ReceiveMessage(dS, msg, clientList); 
        printf("Liste des clients :\n");
        printClients(clientList);
    }

    free(msg);
    freeClients(clientList);
    close(dS);
    return 0;
}