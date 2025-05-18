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
#include "command.h"
#include "users.h"

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
    char password[MAX_PASSWORD_LEN];
};
void sendMessageToClient(struct msgBuffer* msg, int serverSocket, struct sockaddr_in* addr) {
    sendto(serverSocket, msg, sizeof(struct msgBuffer), 0, (struct sockaddr*)addr, sizeof(struct sockaddr_in));
}

int sendMessageToAllClients(ClientNode* clientList, struct msgBuffer* msg, int serverSocket) {
    ClientNode* current = clientList;
    int successCount = 0;
    int errorCount = 0;
    
    while (current != NULL) {
        // Ne pas envoyer au client qui a émis le message
        if (!(current->data.adClient.sin_addr.s_addr == msg->adClient.sin_addr.s_addr &&
            current->data.adClient.sin_port == msg->adClient.sin_port)) { 
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
    c.adClient = adrExp;
    c.port = msg->port;
    c.dSClient = dS;
    strcpy(c.username, msg->username); 
    
    if (!clientAlreadyExists(clientList, c)) { 
        clientList = addClient(clientList, c); 
        printf("Nouveau client ajouté: %s\n", c.username);
    }
    if (msg->opCode == 1) {  
        sendMessageToAllClients(clientList, msg, dS); 
    }
    return clientList; 
}
void handleCommand(Command cmd, struct msgBuffer* msg, int dS, ClientNode** clientList, User* users, int nbUsers) {
    struct msgBuffer response;
    memset(&response, 0, sizeof(response));
    strcpy(response.username, "server");
    response.adClient = msg->adClient;
    response.port = msg->port;

    switch (cmd.type) {
        case CMD_HELP:
            snprintf(response.msg, MAX_MSG_LEN, "Commandes: @help, @ping, @msg <user> <msg>, @credits, @shutdown, @connect <login> <mdp>");
            break;
        case CMD_PING:
            snprintf(response.msg, MAX_MSG_LEN, "pong");
            break;
        case CMD_CREDITS:
            snprintf(response.msg, MAX_MSG_LEN, "Projet Chat UDP - 2025");
            break;
        case CMD_SHUTDOWN:
            snprintf(response.msg, MAX_MSG_LEN, "Serveur en cours d'arrêt...");
            sendMessageToClient(&response, dS, &msg->adClient);
            exit(0);
            break;
        case CMD_CONNECT: {
            int idx = findUser(users, nbUsers, cmd.arg1);
            if (idx >= 0 && checkPassword(users, idx, cmd.arg2)) {
                snprintf(response.msg, MAX_MSG_LEN, "📢 Connexion réussie, bienvenue %s !", cmd.arg1);
                strcpy(msg->username, cmd.arg1);
                if (!clientAlreadyExists(*clientList, *((struct client*)&msg->adClient))) {
                    struct client c;
                    c.adClient = msg->adClient;
                    c.port = msg->port;
                    strcpy(c.username, cmd.arg1);
                    *clientList = addClient(*clientList, c);
                }
            } else {
                snprintf(response.msg, MAX_MSG_LEN, "Erreur d'authentification.");
            }
            break;
        }
        case CMD_MSG: {
            // Message privé
            ClientNode* dest = findClientByName(*clientList, cmd.arg1);
            if (dest) {
                snprintf(response.msg, MAX_MSG_LEN, "[privé de %s]: %s", msg->username, cmd.arg2);
                sendMessageToClient(&response, dS, &dest->data.adClient);
                snprintf(response.msg, MAX_MSG_LEN, "Message privé envoyé à %s.", cmd.arg1);
            } else {
                snprintf(response.msg, MAX_MSG_LEN, "Utilisateur %s introuvable.", cmd.arg1);
            }
            break;
        }
        case CMD_LIST: {
            // Affiche la liste des utilisateurs connectés
            char liste[MAX_MSG_LEN] = "Utilisateurs connectés :";
            ClientNode* cur = *clientList;
            while (cur) {
                strcat(liste, " ");
                strcat(liste, cur->data.username);
                cur = cur->next;
            }
            snprintf(response.msg, MAX_MSG_LEN, "%s", liste);
            break;
        }
        default:
            snprintf(response.msg, MAX_MSG_LEN, "Commande inconnue.");
    }
    sendMessageToClient(&response, dS, &msg->adClient);
}


int main(int argc, char* argv[]) {
    int dS = socket(AF_INET, SOCK_DGRAM, 0);
    if (dS == -1) { perror("socket"); exit(EXIT_FAILURE); }
    struct sockaddr_in adServeur;
    adServeur.sin_family = AF_INET;
    adServeur.sin_addr.s_addr = INADDR_ANY;
    adServeur.sin_port = htons(12345);
    if (bind(dS, (struct sockaddr*)&adServeur, sizeof(adServeur)) == -1) {
        perror("bind"); close(dS); exit(EXIT_FAILURE);
    }

    ClientNode* clientList = NULL;
    User users[MAX_USERS];
    int nbUsers = 0;
    loadUsers("users.txt", users, &nbUsers);

    struct msgBuffer msg;
    while (1) {
        struct sockaddr_in adrExp;
        socklen_t adrExpLen = sizeof(adrExp);
        if (recvfrom(dS, &msg, sizeof(msg), 0, (struct sockaddr*)&adrExp, &adrExpLen) == -1) {
            perror("recvfrom"); continue;
        }
        msg.adClient = adrExp;

        Command cmd = parseCommand(msg.msg);
        if (cmd.type != CMD_UNKNOWN) {
            handleCommand(cmd, &msg, dS, &clientList, users, nbUsers);
        } else {
            // Message normal : broadcast
            sendMessageToAllClients(clientList, &msg, dS);
        }
    }
    freeClients(clientList);
    close(dS);
    return 0;
}