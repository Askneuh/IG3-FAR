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
                msg->opCode = 3;
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
        Command cmd;
        parseCommand(msg->msg, &cmd);

        msg->adClient = adrExp;
        msg->port = adrExp.sin_port;

        traiterCommande(&cmd, msg, dS, &clientList, c);
        sauvegarderSalons();
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

