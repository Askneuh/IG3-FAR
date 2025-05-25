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
#include "common.h"
#include "client_list.h"
#include <pthread.h>
#include "server_mutex.h"
#include "server_com.h"
#include "command.h"
#include "message.h"
#include "salon.h"


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
    

    printf("Message (opcode : %d) reçu de %s : %s\n",msg->opCode, msg->username, msg->msg);
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

    else if (msg->opCode == 2) { 
        printf("Demande de transfert de fichier reçue de %s\n", msg->username);
        

        pthread_mutex_lock(&file_mutex);
        char new_filename[256]; // Assurez-vous que le tableau est assez grand
        snprintf(new_filename, sizeof(new_filename), "serv_%s", msg->msg);
        FILE* receivedFile = fopen(new_filename, "wb");

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

        
        
        struct sockaddr_in tcpAddr = adServeur;
        tcpAddr.sin_port = htons(adrExp.sin_port + 1);

        //renvoyer le port au client
        struct msgBuffer msgPort;
        msgPort.opCode = 5; // OpCode pour l'envoi du port
        msgPort.port = htons(adrExp.sin_port + 1); 
       
        int opt = 1;
        if (setsockopt(dSTCP, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))) {
            perror("setsockopt");
            close(dSTCP);
            pthread_mutex_unlock(&file_mutex);
            return clientList;
        }

        pthread_mutex_lock(&udp_socket_mutex);
        if (sendto(dS, &msgPort, sizeof(struct msgBuffer), 0, (struct sockaddr*)&adrExp, adrExpLen) == -1) {
            perror("Erreur d'envoi du port");
            pthread_mutex_unlock(&udp_socket_mutex);
            close(dSTCP);
            fclose(receivedFile);
            pthread_mutex_unlock(&file_mutex);
            return clientList;
        }
        printf("Port TCP %d envoyé au client\n", msgPort.port);
        pthread_mutex_unlock(&udp_socket_mutex);

        if (bind(dSTCP, (struct sockaddr*)&tcpAddr, sizeof(tcpAddr)) == -1) {
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

       

        listen(dSTCP, 10);

        // Acceptation de la connexion TCP du client
        socklen_t lg = sizeof(adrExp);
        int clientSocketTCP = accept(dSTCP, (struct sockaddr*)&adrExp, &lg);
        printf("Accepté\n");
        
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

    else if (msg->opCode == 8) {  
        printf("commande détectée : \"%s\"\n", msg->msg);
        bool modified = false;    
        if (strncmp(msg->msg, "@create ", 8) == 0) {
            char *nomSalon = msg->msg + 8;
        
            struct msgBuffer response;
            response.opCode = 3;
            response.adClient = c.adClient;
            strcpy(response.username, "Serveur");
        
            if (creerSalon(nomSalon) == 0) {
                modified = true ; 
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
        
        else if (strncmp(msg->msg, "@leave", 6) == 0) {
            modified = true;
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
        
        
        else if (strncmp(msg->msg, "@info ", 6) == 0) {
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
    

        else if (strncmp(msg->msg, "@join ", 6) == 0) {
            modified = true ; 
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
        else if (strncmp(msg->msg, "@who", 4) == 0) {
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

        else if (strncmp(msg->msg, "@broadcast ", 11) == 0) {
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
        else if (strncmp(msg->msg, "@list", 5) == 0) {
            struct msgBuffer rsp = {0};
            rsp.opCode   = 3;             // réponse serveur
            rsp.adClient = c.adClient;    // destinataire
            strcpy(rsp.username, "Serveur");
        
            // Si pas de salon, on prévient
            if (nb_salons == 0) {
                strcpy(rsp.msg, "Aucun salon disponible.");
                sendMessageToOneClient(c, &rsp, dS);
                return clientList;
            }
        
            // Sinon on envoie un salon par message
            for (int i = 0; i < nb_salons; i++) {
                snprintf(rsp.msg, sizeof(rsp.msg),
                         "Salon %d: %s", i+1, salons[i].nom);
                sendMessageToOneClient(c, &rsp, dS);
            }
            return clientList;
        }
        else {
            Command cmd;
            parseCommand(msg->msg, &cmd);
    
            msg->adClient = adrExp;
            msg->port = adrExp.sin_port;
    
            traiterCommande(&cmd, msg, dS, &clientList);
        }

        if (modified) {
            sauvegarderSalons();
        }
        
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
            

            
            
            struct sockaddr_in tcpAddr = adServeur;
            tcpAddr.sin_port = htons(adrExp.sin_port + 2);

            //renvoyer le port au client
            struct msgBuffer msgPort;
            msgPort.opCode = 5; // OpCode pour l'envoi du port
            msgPort.port = htons(adrExp.sin_port + 2); 
            

            int opt = 1;
            if (setsockopt(dSTCP, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))) {
                perror("setsockopt");
                close(dSTCP);
                pthread_mutex_unlock(&file_mutex);
                return clientList;
            }
        

            pthread_mutex_lock(&udp_socket_mutex);
            if (sendto(dS, &msgPort, sizeof(struct msgBuffer), 0, (struct sockaddr*)&adrExp, adrExpLen) == -1) {
                perror("Erreur d'envoi du port");
                pthread_mutex_unlock(&udp_socket_mutex);
                close(dSTCP);
                fclose(file);
                pthread_mutex_unlock(&file_mutex);
                return clientList;
            }
            printf("Port TCP %d envoyé au client\n", msgPort.port);
            pthread_mutex_unlock(&udp_socket_mutex);

            if (bind(dSTCP, (struct sockaddr*)&tcpAddr, sizeof(tcpAddr)) == -1) {
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

       

            listen(dSTCP, 10);

            // Acceptation de la connexion TCP du client
            socklen_t lg = sizeof(adrExp);
            int clientSocketTCP = accept(dSTCP, (struct sockaddr*)&adrExp, &lg);
            printf("Accepté\n");
            
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

    return clientList; 
}

