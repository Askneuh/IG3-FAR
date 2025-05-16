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

#define MAX_USERNAME_LEN 50
#define MAX_MSG_LEN 512

pthread_mutex_t client_list_mutex;
pthread_mutex_t udp_socket_mutex;
pthread_mutex_t file_mutex;

//Choix effectués : échange UDP pour les messages et TCP pour  l'echange de fichiers.

struct msgBuffer {
    char username[MAX_USERNAME_LEN]; //Username du client dont provient le message
    int opCode; //Code pour l'action à réaliser : 1 = message, 2 = fichier
    char msg[MAX_MSG_LEN]; 
    int msgSize;
    int port; // Port de l'envoyeur
    struct sockaddr_in adClient; // Adresse de l'envoyeur
};



int sendMessageToAllClients(ClientNode* clientList, struct msgBuffer* msg, int serverSocket) {
    pthread_mutex_lock(&client_list_mutex);
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
    pthread_mutex_unlock(&client_list_mutex);
    
    printf("Message diffusé à %d clients (%d erreurs)\n", successCount, errorCount);
    return successCount;
}


ClientNode* ReceiveMessage(int dS, struct msgBuffer* msg, ClientNode* clientList, struct sockaddr_in adServeur) {
    struct sockaddr_in adrExp;
    socklen_t adrExpLen = sizeof(adrExp);
    pthread_mutex_lock(&udp_socket_mutex);

    if (recvfrom(dS, msg, sizeof(struct msgBuffer), 0, (struct sockaddr*)&adrExp, &adrExpLen) == -1) { 
        perror("recvfrom");
        close(dS);
        exit(EXIT_FAILURE);
    }
    pthread_mutex_unlock(&udp_socket_mutex);

    struct client c;
    c.adClient = adrExp;
    c.port = msg->port;
    c.dSClient = dS;
    strcpy(c.username, msg->username); 
    

    pthread_mutex_lock(&client_list_mutex);

    if (!clientAlreadyExists(clientList, c)) { 
        clientList = addClient(clientList, c); 
        printf("Nouveau client ajouté: %s\n", c.username);
    }

    pthread_mutex_unlock(&client_list_mutex);


    if (msg->opCode == 1) {  
        sendMessageToAllClients(clientList, msg, dS); 
    }


    else if (msg->opCode == 2) { 
        printf("Demande de transfert de fichier reçue de %s\n", msg->username);
        
        pthread_mutex_lock(&file_mutex);
        
        FILE* receivedFile = fopen(msg->msg, "wb");

        printf("Fichier créé\n");

        // Création de socket TCP du serveur
        int dSTCP = socket(AF_INET, SOCK_STREAM, 0);
        if (dSTCP == -1) {
            pthread_mutex_lock(&udp_socket_mutex);
            perror("Erreur création socket TCP");
            strcpy(msg->msg, "Erreur serveur: impossible de créer une socket TCP");
            pthread_mutex_lock(&udp_socket_mutex);
            sendto(dS, msg, sizeof(struct msgBuffer), 0, (struct sockaddr*)&adrExp, adrExpLen);
            pthread_mutex_unlock(&udp_socket_mutex);
            pthread_mutex_unlock(&file_mutex);
            return clientList;
        }
        printf("Socket TCP serv créée\n");

        
        
        // Bind de la socket TCP
        if (bind(dSTCP, (struct sockaddr*)&adServeur, sizeof(adServeur)) == -1) {
            perror("Erreur bind socket TCP");
            close(dSTCP);
            strcpy(msg->msg, "Erreur serveur: impossible de bind la socket TCP");
            pthread_mutex_lock(&udp_socket_mutex);
            sendto(dS, msg, sizeof(struct msgBuffer), 0, (struct sockaddr*)&adrExp, adrExpLen);
            pthread_mutex_unlock(&udp_socket_mutex);
            pthread_mutex_unlock(&file_mutex);
            return clientList;
        }

        printf("Binded\n");
        
        // Mise en écoute de la socket TCP
        if (listen(dSTCP, 5) == -1) {
            perror("Erreur listen socket TCP");
            close(dSTCP);
            pthread_mutex_unlock(&file_mutex);
            return clientList;
        }
            
        printf("En attente de connexion TCP du client...\n");
        // Acceptation de la connexion TCP du client
        socklen_t lg = sizeof(adrExp);
        int clientSocketTCP = accept(dSTCP, (struct sockaddr*)&adrExp, &lg);
        
        if (clientSocketTCP == -1) {
            perror("❌ Erreur accept");
            close(dSTCP);
            pthread_mutex_unlock(&file_mutex);
            return clientList;
        }
        char buffer[MAX_MSG_LEN];
        ssize_t bytesReceived;
        while ((bytesReceived = recv(clientSocketTCP, buffer, MAX_MSG_LEN, 0)) > 0) {
            if (fwrite(buffer, 1, bytesReceived, receivedFile) != (size_t)bytesReceived) {
                perror("Erreur lors de l'écriture dans le fichier");
                close(clientSocketTCP);
                close(dSTCP);
            }
        }
        fclose(receivedFile);
        
        if (bytesReceived < 0) {
            perror("Erreur lors de la réception des données");
        } 
        else {
            printf("Fichier reçu avec succès.\n");
        }

        pthread_mutex_unlock(&file_mutex);
       
        
        close(clientSocketTCP);
        close(dSTCP);
    } 
    else if (msg->opCode == 6) { 
        printf("Demande de download de fichier reçue de %s\n", msg->username);
        pthread_mutex_lock(&file_mutex);
        bool fileExists = true;
        char* filename[MAX_MSG_LEN];
        strcpy(filename, msg->msg);
        FILE *file = fopen(filename, "rb");
        if (!file) {
            perror("❌ Erreur d'ouverture du fichier");
            printf("Le fichier n'existe pas ou n'est pas accessible\n");
            fileExists = false;
        }


        if (fileExists) {
            // Création de socket TCP du serveur
            int dSTCP = socket(AF_INET, SOCK_STREAM, 0);
            if (dSTCP == -1) {
                perror("Erreur création socket TCP");
                strcpy(msg->msg, "Erreur serveur: impossible de créer une socket TCP");
                pthread_mutex_lock(&udp_socket_mutex);
                sendto(dS, msg, sizeof(struct msgBuffer), 0, (struct sockaddr*)&adrExp, adrExpLen);
                pthread_mutex_unlock(&udp_socket_mutex);
                pthread_mutex_unlock(&file_mutex);
                return clientList;
            }
            

            
            
            // Bind de la socket TCP
            if (bind(dSTCP, (struct sockaddr*)&adServeur, sizeof(adServeur)) == -1) {
                perror("Erreur bind socket TCP");
                close(dSTCP);
                strcpy(msg->msg, "Erreur serveur: impossible de bind la socket TCP");
                pthread_mutex_lock(&udp_socket_mutex);
                sendto(dS, msg, sizeof(struct msgBuffer), 0, (struct sockaddr*)&adrExp, adrExpLen);
                pthread_mutex_unlock(&udp_socket_mutex);
                pthread_mutex_unlock(&file_mutex);
                return clientList;
            }

            printf("Binded\n");
            
            // Mise en écoute de la socket TCP
            if (listen(dSTCP, 5) == -1) {
                perror("Erreur listen socket TCP");
                close(dSTCP);
                pthread_mutex_unlock(&file_mutex);
                return clientList;
            }
                
            printf("En attente de connexion TCP du client...\n");
            // Acceptation de la connexion TCP du client
            socklen_t lg = sizeof(adrExp);
            int clientSocketTCP = accept(dSTCP, (struct sockaddr*)&adrExp, &lg);
            
            if (clientSocketTCP == -1) {
                perror("❌ Erreur accept");
                close(dSTCP);
                pthread_mutex_unlock(&file_mutex);
                return clientList;
            }
            
            // Envoi du fichier via TCP
            ssize_t bytesRead;
            char buffer[MAX_MSG_LEN];
            while ((bytesRead = fread(buffer, 1, MAX_MSG_LEN, file)) > 0) {
                if (send(clientSocketTCP, buffer, bytesRead, 0) < 0) {
                    perror("Erreur lors de l'envoi des données");
                    close(dSTCP);
                    fclose(file);
                }
                printf("Envoi de %ld octets au serveur.", bytesRead);
            }
            
            fclose(file);
            close(clientSocketTCP);
            close(dSTCP);
        
            if (bytesRead < 0) {
                perror("Erreur lors de l'envoi");
            } 
            else {
                printf("Fichier envoyé avec succès.\n");
            }
            pthread_mutex_unlock(&file_mutex);
        }
        return clientList;
    } 
    else {
        printf("OpCode inconnu: %d\n", msg->opCode);
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
    bool continueReceiving = true;
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
    pthread_mutex_destroy(&client_list_mutex);
    pthread_mutex_destroy(&udp_socket_mutex);
    pthread_mutex_destroy(&file_mutex);
    free(msg);
    freeClients(clientList);
    close(dS);
    return 0;
}