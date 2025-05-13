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
#include "salon.h"
#include "message.h"



//Choix effectués : échange UDP pour les messages et TCP pour  l'echange de fichiers.



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

    if(strncmp(msg->msg, "@create ", 8) == 0){
        char *nomSalon = msg->msg + 8; // On saute "@create "
        if (creerSalon(nomSalon) == 0) {
            struct msgBuffer response;
            strcpy(response.msg, "✅ Salon créé.");
            response.opCode = 3;
            response.adClient = c.adClient;
            sendMessageToOneClient(c, &response, dS);
            ajouterClientAuSalon(nomSalon, c);
            afficherinfoSalon(nomSalon);
        } else {
            send(dS, " Erreur création salon.\n", strlen("Erreur création salon.\n"), 0);
        }
    }

    if (strncmp(msg->msg, "@info ", 6) == 0) {
        char *nomSalon = msg->msg + 6;
        int idx = salonExiste(nomSalon);
        struct msgBuffer response;
        response.opCode = 3;
        response.adClient = c.adClient;
        strcpy(response.username, "Serveur");
        if (idx == -1) {
            strcpy(response.msg, "❌ Salon introuvable.");
        } else {
            Salon s = salons[idx];
            char info[512] = "";
            sprintf(info, "📂 Salon \"%s\" (%d membres):\n", s.nom, s.nb_clients);
            for (int i = 0; i < s.nb_clients; i++) {
                strcat(info, " - ");
                strcat(info, s.clients[i].username);
                strcat(info, "\n");
            }
            strcpy(response.msg, info);
        }
    
        sendMessageToOneClient(c, &response, dS);
    }
   

    if (strncmp(msg->msg, "@join ", 6) == 0) {
        char *nomSalon = msg->msg + 6;
    
        struct msgBuffer response;
        response.opCode = 3;
        response.adClient = c.adClient;
        strcpy(response.username, "Serveur");
    
        int idx = salonExiste(nomSalon);
        if (idx == -1) {
            strcpy(response.msg, "❌ Salon introuvable.");
            sendMessageToOneClient(c, &response, dS);
        } else {
            int result = ajouterClientAuSalon(nomSalon, c);
            if (result == 0) {
                sprintf(response.msg, "✅ Tu as rejoint le salon \"%s\".", nomSalon);
                sendMessageToOneClient(c, &response, dS);
    
                //  Notifier les autres membres du salon
                struct msgBuffer notif;
                notif.opCode = 3;
                notif.adClient = c.adClient;
                strcpy(notif.username, "Serveur");
                snprintf(notif.msg, sizeof(notif.msg), " %s a rejoint le salon \"%s\".", c.username, nomSalon);
                envoyerMessageAListe(nomSalon, &notif, dS);
                printf("client \"%s\" a rejoint le salon \"%s\"",c.username,nomSalon);
            } else if (result == -2) {
                sprintf(response.msg, "⚠️ Tu es déjà dans le salon \"%s\".", nomSalon);
                sendMessageToOneClient(c, &response, dS);
            } else {
                sprintf(response.msg, "❌ Impossible de rejoindre \"%s\" (plein ?).", nomSalon);
                sendMessageToOneClient(c, &response, dS);
            }
        }
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