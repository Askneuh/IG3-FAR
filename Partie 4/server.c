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
    if (recvfrom(dS, msg, sizeof(*msg), 0, (struct sockaddr*)&adrExp, &adrExpLen) == -1) {
        perror("recvfrom");
        close(dS);
        exit(EXIT_FAILURE);
    }
    printf("Received message from %s\n", msg->username);
    printf("Message reçu : %s\n", msg->msg);

    struct client c;
    c.adClient   = adrExp;
    c.port       = msg->port;
    c.dSClient   = dS;
    strcpy(c.username, msg->username);

    // 1) On ajoute à la liste globale si c’est un nouveau peer
    if (!clientAlreadyExists(clientList, c)) {
        clientList = addClient(clientList, c);
        printf("➕ Nouveau client ajouté: %s\n", c.username);


    }
    
    // 🔁 Toujours exécuter le traitement du message ensuite
    if (msg->opCode == 1) {
        char* nomSalon = trouverSalonDuClient(c);
        printf("nom du salon client : \"%s\"\n", nomSalon ? nomSalon : "NULL");
        if (nomSalon == NULL) {
            envoyerMessageAListe(clientList, msg, dS, &c);  // si tu veux exclure l'émetteur
        } else {
            int idx = salonExiste(nomSalon);
            if (idx != -1) {
                strcpy(msg->username, c.username);
                msg->opCode = 7;
                envoyerMessageAListe(salons[idx].clients, msg, dS, &c);
            }
        }
    }
    
    if (msg->opCode == 4) {  
        printf("commande détectée : \"%s\"\n", msg->msg);    
        if (strncmp(msg->msg, "@create ", 8) == 0) {
            char *nomSalon = msg->msg + 8;
        
            struct msgBuffer response;
            response.opCode = 3;
            response.adClient = c.adClient;
            strcpy(response.username, "Serveur");
        
            if (creerSalon(nomSalon) == 0) {
                // ✅ Retirer l’utilisateur d’un éventuel salon existant
                char* salonActuel = trouverSalonDuClient(c);

                printf("🔍 Salon actuel du client %s : %s\n", c.username, salonActuel ? salonActuel : "Aucun");

                if (salonActuel != NULL) {
                    retirerClientDuSalon(salonActuel, c);
                    printf("✅ Client retiré de %s\n", salonActuel);
                    
        
                    // Notif aux autres
                    int idxOld = salonExiste(salonActuel);
                    if (idxOld != -1) {
                        struct msgBuffer notifLeave;
                        notifLeave.opCode = 3;
                        notifLeave.adClient = c.adClient;
                        strcpy(notifLeave.username, "Serveur");
                        snprintf(notifLeave.msg, sizeof(notifLeave.msg), "📢 %s a quitté le salon \"%s\".", c.username, salonActuel);
                        envoyerMessageAListe(salons[idxOld].clients, msg, dS, &c);
                    }
                }
        
                // ✅ Ajouter au nouveau salon
                ajouterClientAuSalon(nomSalon, c);
                afficherinfoSalon(nomSalon);
        
                // ✅ Envoyer message de succès
                strcpy(response.msg, "✅ Salon créé et rejoint.");
                sendMessageToOneClient(c, &response, dS);
            } else {
                strcpy(response.msg, "❌ Erreur : salon déjà existant ou limite atteinte.");
                sendMessageToOneClient(c, &response, dS);
            }
        }
        
        if (strncmp(msg->msg, "@leave", 6) == 0) {
            char* nomSalon = trouverSalonDuClient(c);
        
            struct msgBuffer response;
            response.opCode = 3;
            response.adClient = c.adClient;
            strcpy(response.username, "Serveur");
        
            if (nomSalon == NULL) {
                strcpy(response.msg, "❌ Tu n'es dans aucun salon.");
                sendMessageToOneClient(c, &response, dS);
            } else {
                // ✅ Retirer le client
                retirerClientDuSalon(nomSalon, c);
        
                // ✅ Message au client
                sprintf(response.msg, "✅ Tu as quitté le salon \"%s\".", nomSalon);
                sendMessageToOneClient(c, &response, dS);
        
                // ✅ Notifier les autres
                struct msgBuffer notif;
                notif.opCode = 3;
                notif.adClient = c.adClient;
                strcpy(notif.username, "Serveur");
                snprintf(notif.msg, sizeof(notif.msg), "📢 %s a quitté le salon \"%s\".", c.username, nomSalon);
        
                int idx = salonExiste(nomSalon);
                if (idx != -1) {
                    envoyerMessageAListe(salons[idx].clients, msg, dS, &c);
                }
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
            } else {sendMessageToOneClient(c, &response, dS); // ✅ la bonne fonction

                Salon s = salons[idx];
                char info[512] = "";
                sprintf(info, "📂 Salon \"%s\" (%d membres):\n", s.nom, countClients(s.clients)
            );
                ClientNode* current = salons[idx].clients;
                while (current != NULL) {
                    strcat(info, " - ");
                    strcat(info, current->data.username);
                    strcat(info, "\n");
                    current = current->next;
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
        
            // 🔎 Vérifier si le client est déjà dans un salon
            char* salonActuel = trouverSalonDuClient(c);
            if (salonActuel != NULL && strcmp(salonActuel, nomSalon) != 0) {
                // ✅ Le client est dans un autre salon → on le retire
                retirerClientDuSalon(salonActuel, c);
        
                // 📢 Notifier les autres
                struct msgBuffer notifLeave;
                notifLeave.opCode = 3;
                notifLeave.adClient = c.adClient;
                strcpy(notifLeave.username, "Serveur");
                snprintf(notifLeave.msg, sizeof(notifLeave.msg), "📢 %s a quitté le salon \"%s\".", c.username, salonActuel);
                int idxLeave = salonExiste(salonActuel);
                if (idxLeave != -1) {
                    envoyerMessageAListe(salons[idxLeave].clients, msg, dS, &c);

                }
            }
        
            // 🔁 Continuer avec la logique join
            int idx = salonExiste(nomSalon);
            if (idx == -1) {
                strcpy(response.msg, "❌ Salon introuvable.");
                sendMessageToOneClient(c, &response, dS);
            } else {
                int result = ajouterClientAuSalon(nomSalon, c);
                if (result == 0) {
                    sprintf(response.msg, "✅ Tu as rejoint le salon \"%s\".", nomSalon);
                    sendMessageToOneClient(c, &response, dS);
        
                    // ✅ Notifier les autres
                    struct msgBuffer notif;
                    notif.opCode = 3;
                    notif.adClient = c.adClient;
                    strcpy(notif.username, "Serveur");
                    snprintf(notif.msg, sizeof(notif.msg), "📢 %s a rejoint le salon \"%s\".", c.username, nomSalon);
                    envoyerMessageAListe(salons[idx].clients, msg, dS, &c);
                } else if (result == -2) {
                    sprintf(response.msg, "⚠️ Tu es déjà dans le salon \"%s\".", nomSalon);
                    sendMessageToOneClient(c, &response, dS);
                } else {
                    sprintf(response.msg, "❌ Impossible de rejoindre \"%s\" (plein ?).", nomSalon);
                    sendMessageToOneClient(c, &response, dS);
                }
            }
        }
        if (strncmp(msg->msg, "@who", 4) == 0) {
            struct msgBuffer response;
            response.opCode = 3;
            response.adClient = c.adClient;
            strcpy(response.username, "Serveur");
        
            char* nomSalon = trouverSalonDuClient(c);
            if (nomSalon == NULL) {
                strcpy(response.msg, "❌ Tu n'es dans aucun salon.");
            } else {
                int idx = salonExiste(nomSalon);
                if (idx == -1) {
                    strcpy(response.msg, "❌ Salon introuvable.");
                } else {
                    Salon s = salons[idx];
                    char info[512] = "";
                    snprintf(info, sizeof(info), "👥 Membres du salon \"%s\" :\n", s.nom);
                    ClientNode* current = salons[idx].clients;
                    while (current != NULL) {
                        strcat(info, " - ");
                        strcat(info, current->data.username);
                        strcat(info, "\n");
                        current = current->next;
                    }

                    strcpy(response.msg, info);
                }
            }
        
            sendMessageToOneClient(c, &response, dS);
        }

        if (strncmp(msg->msg, "@broadcast ", 11) == 0) {
            char *texte = msg->msg + 11;
        
            // Construction du message
            struct msgBuffer broadcast;
            broadcast.opCode = 1;  // message global
            broadcast.adClient = c.adClient;
            strcpy(broadcast.username, c.username);
            strncpy(broadcast.msg, texte, sizeof(broadcast.msg));
            broadcast.msgSize = strlen(broadcast.msg) + 1;
        
            sendMessageToAllClients(clientList, &broadcast, dS);
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
    chargerSalons();
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