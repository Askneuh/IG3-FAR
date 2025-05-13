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

struct fileBuffer {
    char filename[MAX_MSG_LEN];
    int fileSize;
    char fileData[MAX_MSG_LEN];
};

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


ClientNode* ReceiveMessage(int dS, struct msgBuffer* msg, ClientNode* clientList, struct sockaddr_in adServeur) {

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
    else if (msg->opCode == 2) { 
    printf("🔄 Demande de transfert de fichier reçue de %s\n", msg->username);
    
    // Création d'une socket TCP
    int dSTCP = socket(AF_INET, SOCK_STREAM, 0);
    if (dSTCP == -1) {
        perror("❌ Erreur création socket TCP");
        // On envoie un message d'erreur au client
        strcpy(msg->msg, "Erreur serveur: impossible de créer une socket TCP");
        sendto(dS, msg, sizeof(struct msgBuffer), 0, (struct sockaddr*)&adrExp, adrExpLen);
        return clientList;
    }
    
    // Configuration de l'adresse de la socket TCP
    struct sockaddr_in serverTCP;
        serverTCP.sin_family = AF_INET;
        serverTCP.sin_addr.s_addr = adServeur.sin_addr.s_addr;
        serverTCP.sin_port = htons(12346); // Port TCP du serveur
    
    // Options pour réutiliser l'adresse et le port
    int opt = 1;
    if (setsockopt(dSTCP, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("❌ Erreur setsockopt");
        close(dSTCP);
        return clientList;
    }
    
    // Bind de la socket TCP
    if (bind(dSTCP, (struct sockaddr*)&serverTCP, sizeof(serverTCP)) == -1) {
        perror("❌ Erreur bind socket TCP");
        close(dSTCP);
        // On envoie un message d'erreur au client
        strcpy(msg->msg, "Erreur serveur: impossible de bind la socket TCP");
        sendto(dS, msg, sizeof(struct msgBuffer), 0, (struct sockaddr*)&adrExp, adrExpLen);
        return clientList;
    }
    
    // Mise en écoute de la socket TCP
    if (listen(dSTCP, 5) == -1) {
        perror("❌ Erreur listen socket TCP");
        printf("DEBUG: dSTCP=%d, errno=%d (%s)\n", dSTCP, errno, strerror(errno));
        close(dSTCP);
        return clientList;
    }
    
    // Préparation de la réponse au client avec les informations de connexion TCP
    msg->port = htons(12346); // Port TCP du serveur
    sendto(dS, msg, sizeof(struct msgBuffer), 0, (struct sockaddr*)&adrExp, adrExpLen);
    printf("📤 Informations de connexion TCP envoyées au client\n");
    
    printf("⏳ En attente de connexion TCP du client...\n");
    // Acceptation de la connexion TCP du client
    struct sockaddr_in addrClient;
    socklen_t addrClientLen = sizeof(addrClient);
    int newSocket = accept(dSTCP, (struct sockaddr*)&addrClient, &addrClientLen);
    
    if (newSocket == -1) {
        perror("❌ Erreur accept");
        close(dSTCP);
        return clientList;
    }
    printf("🔗 Client connecté sur la socket TCP\n");
    
    // Réception du fichier
    struct fileBuffer fileData;
    int totalReceived = 0;
    char outputPath[MAX_MSG_LEN + 15]; // Pour stocker le chemin du fichier reçu
    FILE* file = NULL;
    bool continueReceiving = true;
    while (continueReceiving) {
        // Réception des données
        ssize_t bytesReceived = recv(newSocket, &fileData, sizeof(fileData), 0);
        
        if (bytesReceived <= 0) {
            if (bytesReceived == 0) {
                printf("🔌 Connexion fermée par le client\n");
                continueReceiving = false; // On arrête la réception
            } else {
                perror("❌ Erreur recv");
                continueReceiving = false; // On arrête la réception
            }
            
        }
        
        // Si c'est le premier paquet ou si le fichier n'est pas encore ouvert
        if (file == NULL) {
            // Création du nom de fichier unique avec préfixe "received_"
            snprintf(outputPath, sizeof(outputPath), "received_%s", fileData.filename);
            
            // Ouverture du fichier en écriture
            file = fopen(outputPath, "wb");
            if (!file) {
                perror("❌ Erreur ouverture fichier");
                close(newSocket);
                close(dSTCP);
                return clientList;
            }
            printf("📂 Fichier '%s' créé pour la réception\n", outputPath);
            printf("Contenu du fichier : %s\n", fileData.fileData);
        }
        
        // Vérifier si c'est le dernier paquet (taille = 0)
        if (fileData.fileSize == 0) {
            printf("✅ Transfert terminé\n");
            continueReceiving = false; // On arrête la réception
        }
        
        // Écriture des données dans le fichier
        size_t writtenBytes = fwrite(fileData.fileData, 1, fileData.fileSize, file);
        if (writtenBytes != fileData.fileSize) {
            perror("❌ Erreur écriture fichier");
            continueReceiving = false; // On arrête la réception
        }
        
        totalReceived += fileData.fileSize;
        printf("📥 Réception en cours: %d octets reçus\n", totalReceived);
    }
    
    // Fermeture du fichier si ouvert
    if (file) {
        fclose(file);
        printf("📄 Fichier '%s' reçu avec succès (%d octets)\n", outputPath, totalReceived);
        
        // Notification aux autres clients qu'un fichier a été reçu
        sprintf(msg->msg, "Le fichier '%s' a été envoyé par %s", fileData.filename, msg->username);
        msg->opCode = 1; // On utilise l'opCode standard pour envoyer un message
        sendMessageToAllClients(clientList, msg, dS);
    }
    
    // Fermeture des sockets
    close(newSocket);
    close(dSTCP);
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
        printf("Liste des clients:\n");
        printClients(clientList);
        if (strcmp(msg->msg, "quit") == 0) {
            printf("Arrêt du serveur demandé.\n");
            continueReceiving = false;
        }
    }

    free(msg);
    freeClients(clientList);
    close(dS);
    return 0;
}