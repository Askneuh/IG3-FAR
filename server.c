#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

//Choix effectués : échange UDP pour les messages et TCP pour  l'echange de fichiers.
struct client { //On en a besoin pour créer une socket pour chaque client
    char* username;
    struct sockaddr_in adClient;
    int port; // Port du client
    int dSClient; // socket du client
}

struct msgBuffer {
    char* username;
    int opCode; 
    char* msg;
    int msgSize;
    int port; // Port du client
    struct sockaddr_in adClient; // Adresse du client
}

void addClient(struct client* clients, struct sockaddr_in adClient, int dSClient) {
    // On ajoute le client à la liste des clients
    for (int i = 0; i < 10; i++) { // On suppose qu'on a 10 clients max
        if (clients[i].username == NULL) { // Si le client n'est pas déjà dans la liste
            clients[i].adClient = adClient; // On copie l'adresse du client
            clients[i].dSClient = dSClient; // On copie la socket du client
            break;
        }
    }
}

void clientAlreadyExists(struct client* clients, struct sockaddr_in adClient) {
    // On vérifie si le client est déjà dans la liste
    for (int i = 0; i < 10; i++) { // On suppose qu'on a 10 clients max
        if (clients[i].adClient.sin_addr.s_addr == adClient.sin_addr.s_addr && clients[i].adClient.sin_port == adClient.sin_port) {
            printf("Client already exists\n");
            return true;
        }
    }
    printf("Client does not exist\n");
    return false;
}
void sendMessageToAllClients(struct client* clients, struct msgBuffer* msg) {
    // On envoie le message à tous les clients
    for (int i = 0; i < 10; i++) { // On suppose qu'on a 10 clients max
        if (clients[i].username != NULL) { // Si le client est dans la liste
            send(clients[i].dSClient, msg, sizeof(struct msgBuffer), 0); // On envoie le message au client
        }
    }
}

void initClients(struct client* clients) {
    // On initialise la liste des clients
    for (int i = 0; i < 10; i++) { // On suppose qu'on a 10 clients max
        clients[i].username = NULL; // On initialise le nom d'utilisateur à NULL
        clients[i].port = 0; // On initialise le port à 0
        clients[i].dSClient = -1; // On initialise la socket à -1
    }
}

void ReceiveMessage(int dS, struct msgBuffer* msg, clients* clients) {
    int res = recv(dS, msg, sizeof(struct msgBuffer), 0); // On reçoit le message
    if (res == -1) {
        perror("recv");
        close(dS);
        exit(EXIT_FAILURE);
    }
    if (res == 0) {
        printf("Client disconnected\n");
        close(dS);
        exit(EXIT_FAILURE);
    }
    if (!clientAlreadyExists(clients, msg->adClient)) { // Si le client n'est pas déjà dans la liste
        addClient(clients, msg->adClient, dS); // On l'ajoute à la liste des clients
    }

    if msg->opCode == 1 { // Si c'est un message texte
        char* textmsg = malloc(msg->msgSize); // On alloue de la mémoire pour le message
        textmsg = msg->msg; // On copie le message dans la mémoire allouée
        if (textmsg == NULL) {
            perror("malloc");
            close(dS);
            exit(EXIT_FAILURE);
        }
        sendMessageToAllClients(clients, msg); // On envoie le message à tous les clients
    }
    return;
}

int main(int argc, char* argv[]) {
    int dS = socket(AF_INET, SOCK_DGRAM, 0); // Création de la socket UDP
    if (dS == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in adServeur;
    adServeur.sin_family = AF_INET; 
    adServeur.sin_addr.s_addr = INADDR_ANY; 
    adServeur.sin_port = htons(12345); 
    if (bind(dS, (struct sockaddr*)&adServeur, sizeof(adServeur)) == -1) { 
        perror("bind");
        close(dS);
        exit(EXIT_FAILURE);
    }
    client* clients = malloc(sizeof(client) * 10); // On alloue de la mémoire pour 10 clients
    initClients(clients); // On initialise la liste des clients
    ReceiveMessage(dS, msg, clients); // On reçoit le message
    close(dS); // On ferme la socket
}