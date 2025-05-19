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
#include "users.h"
#include "command.h"
#include "message.h"

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


ClientNode* ReceiveMessage(int dS, struct msgBuffer* msg, ClientNode* clientList, User* users, int nbUsers, struct sockaddr_in adServeur) {
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

    printf("Message (opcode : %d) reçu de %s : %s\n",msg->opCode, msg->username, msg->msg);
    printf("Adresse du client : %s:%d\n", inet_ntoa(adrExp.sin_addr), ntohs(adrExp.sin_port));
    if (msg->opCode == 1) {  
        sendMessageToAllClients(clientList, msg, dS); 
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
     else if (msg->opCode > 8) {  
        // Commandes/chat
        Command cmd = parseCommand(msg->msg);
        if (cmd.type != CMD_UNKNOWN) {
            handleCommand(cmd, msg, dS, &clientList, users, nbUsers, adrExp);
        } else {
            sendMessageToAllClients(clientList, msg, dS); 
        }
    }
    else {
        printf("OpCode inconnu: %d\n", msg->opCode);
    }
    return clientList; 
}

void handleCommand(Command cmd, struct msgBuffer* msg, int dS, ClientNode** clientList, User* users, int nbUsers, struct sockaddr_in adrExp) {
    struct msgBuffer response;
    memset(&response, 0, sizeof(response));
    strcpy(response.username, "server");
    response.adClient = msg->adClient;
    response.port = msg->port;
    response.opCode = 9; // OpCode pour le message de réponse aux commandes

    switch (cmd.type) {
        case CMD_HELP:
            strcpy(response.msg, "Commandes: @help, @ping, @msg <user> <msg>, @credits, @shutdown, @connect <login> <mdp>");
            //snprintf(response.msg, MAX_MSG_LEN, "Commandes: @help, @ping, @msg <user> <msg>, @credits, @shutdown, @connect <login> <mdp>");
            break;
        case CMD_PING:
            //snprintf(response.msg, MAX_MSG_LEN, "pong");
            strcpy(response.msg, "pong");
            break;
        case CMD_CREDITS:
            //snprintf(response.msg, MAX_MSG_LEN, "Projet Chat UDP - 2025");
            strcpy(response.msg, "Projet Chat UDP - 2025");
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
                if (!clientAlreadyExists(*clientList, *((struct client*)&adrExp))) {
                    struct client c;
                    c.adClient = adrExp;
                    c.port = ntohs(adrExp.sin_port);
                    strcpy(c.username, cmd.arg1);
                    *clientList = addClient(*clientList, c);
                }
            } else if (idx >= 0) {
                snprintf(response.msg, MAX_MSG_LEN, "Erreur d'authentification.");
            } else {
                snprintf(response.msg, MAX_MSG_LEN, "Utilisateur inconnu. Utilisez @create <pseudo> <mdp> pour créer un compte.");
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
       case CMD_CREATE: {
            int idx = findUser(users, nbUsers, cmd.arg1);
            if (idx >= 0) {
                snprintf(response.msg, MAX_MSG_LEN, "Ce pseudo existe déjà.");
            } else {
            // Ajout dans users.txt
                FILE* f = fopen("users.txt", "a");
                if (f) {
                    fprintf(f, "%s %s\n", cmd.arg1, cmd.arg2);
                    fclose(f);
                    snprintf(response.msg, MAX_MSG_LEN, "Compte créé, bienvenue %s !", cmd.arg1);
                    // Ajout dans la liste en mémoire
                    strcpy(users[nbUsers].username, cmd.arg1);
                    strcpy(users[nbUsers].password, cmd.arg2);
                    nbUsers++;
                    // Ajout dans la liste des clients connectés
                    if (!clientAlreadyExists(*clientList, *((struct client*)&adrExp))) {
                        struct client c;
                        c.adClient = adrExp;
                        c.port = ntohs(adrExp.sin_port);
                        strcpy(c.username, cmd.arg1);
                        *clientList = addClient(*clientList, c);
                    }
                } else {
                    snprintf(response.msg, MAX_MSG_LEN, "Erreur lors de la création du compte.");
                }
            }
            break;
        }
        default:
            snprintf(response.msg, MAX_MSG_LEN, "Commande inconnue.");
    }
    printf("envoie avec le opCode %d\n", response.opCode);
    sendMessageToClient(&response, dS, &adrExp);
}