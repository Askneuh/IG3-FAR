#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include "common.h"
#include "file_handler.h"
#include "client_threads.h"
#include "variables.h"
#include "client_mutex.h"



void upload_file(char *filename, char* username, int dS, struct sockaddr_in aS, struct sockaddr_in aD) {
    bool fileExists = true;
    
    pthread_mutex_lock(&file_mutex);
    
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("❌ Erreur d'ouverture du fichier");
        printf("Le fichier '%s' n'existe pas ou n'est pas accessible\n", filename);
        fileExists = false;
    }
    pthread_mutex_unlock(&file_mutex);

    if (fileExists) {
        struct msgBuffer m;
        strcpy(m.username, username);
        m.opCode = 2; // OpCode pour l'envoi de fichier
        m.port = htons(aD.sin_port);
        m.adClient = aD;
        strcpy(m.msg, filename);
        //Demande d'envoi de fichier
        pthread_mutex_lock(&udp_socket_mutex);
        if (sendto(dS, &m, sizeof(m), 0, (struct sockaddr*)&aS, sizeof(aS)) == -1) {
            perror("❌ Erreur d'envoi de la requête");
            pthread_mutex_unlock(&udp_socket_mutex);
            pthread_mutex_unlock(&file_mutex);
            printf("Unlock file\n");
            fclose(file);
            return;
        }
        pthread_mutex_unlock(&udp_socket_mutex);
        
        printf("📤 Requête d'envoi de fichier envoyée pour '%s'\n", filename);
        
        // Création de la socket TCP du client
        int dSTCP = socket(PF_INET, SOCK_STREAM, 0);
        if (dSTCP == -1) {
            perror("❌ Erreur de création de la socket TCP");
            fclose(file);
            pthread_mutex_unlock(&file_mutex);
            printf("Unlock file\n");
            return;
        }
        

        // Configuration pour recevoir le port TCP du serveur
        pthread_mutex_lock(&opcode_mutex);
        expected_opcode = 5;  // OpCode pour le message de port TCP
        msg_received = 0;
        pthread_mutex_unlock(&opcode_mutex);
        
        // Attente du message avec le port TCP
        int timeout_counter = 0;
        while (!msg_received && timeout_counter < 50) {  // attendre max 5 secondes
            usleep(100000);  // 100ms
            timeout_counter++;
        }
        
        pthread_mutex_lock(&opcode_mutex);
        if (!msg_received) {
            perror("❌ Timeout - Pas reçu de port TCP");
            expected_opcode = 0;
            pthread_mutex_unlock(&opcode_mutex);
            close(dSTCP);
            fclose(file);
            return;
        }
        
        // Récupération du port
        int port_tcp = ntohs(expected_msg.port);
        expected_opcode = 0;  // Reset pour les futurs messages
        msg_received = 0;
        pthread_mutex_unlock(&opcode_mutex);
        
        
        struct sockaddr_in tcpServerAddr = aS;
        tcpServerAddr.sin_port = htons(port_tcp);

        if (connect(dSTCP, (struct sockaddr*)&tcpServerAddr, sizeof(tcpServerAddr)) == -1) {
            perror("❌ Erreur de connexion TCP au serveur");
            close(dSTCP);
            fclose(file);
            pthread_mutex_unlock(&file_mutex);
            printf("Unlock file\n");
            return;
        }
        printf("Connexion à la socket TCP établie\n");
        
        pthread_mutex_lock(&file_mutex);
        
        // Envoi du fichier via TCP
        ssize_t bytesRead;
        char buffer[MAX_MSG_LEN];
        while ((bytesRead = fread(buffer, 1, MAX_MSG_LEN, file)) > 0) {
            if (send(dSTCP, buffer, bytesRead, 0) < 0) {
                perror("Erreur lors de l'envoi des données");
                close(dSTCP);
                fclose(file);
                pthread_mutex_unlock(&file_mutex);
                printf("Unlock file\n");
                return;
            }
            printf("Envoi de %ld octets au serveur.\n", bytesRead);
        }
    
        fclose(file);
        close(dSTCP);
        pthread_mutex_unlock(&file_mutex);
        printf("✅ Fichier envoyé avec succès!\n");
    }
}

void download_file(char* filename, char* username, int dS, struct sockaddr_in aS, struct sockaddr_in aD) {
    pthread_mutex_lock(&file_mutex);
    printf("Lock File\n");
    char new_filename[256]; // Assurez-vous que le tableau est assez grand
    snprintf(new_filename, sizeof(new_filename), "new_%s", filename);
    FILE* file = fopen(new_filename, "wb");
    if (!file) {
        perror("❌ Erreur d'ouverture du fichier en écriture");
        pthread_mutex_unlock(&file_mutex);
    }
    
    struct msgBuffer m;
    strcpy(m.username, username);
    m.opCode = 6; // OpCode pour la demande de fichier
    m.port = htons(aD.sin_port);
    m.adClient = aD;
    strcpy(m.msg, filename);

    // Demande de reception du fichier 
    pthread_mutex_lock(&udp_socket_mutex);
    printf("Lock UDP\n");
    if (sendto(dS, &m, sizeof(m), 0, (struct sockaddr*)&aS, sizeof(aS)) == -1) {
        perror("❌ Erreur d'envoi de la requête");
        pthread_mutex_unlock(&udp_socket_mutex);
        pthread_mutex_unlock(&file_mutex);
        fclose(file);
    }
    pthread_mutex_unlock(&udp_socket_mutex);
    printf("Unlock UDP\n");
    printf("📤 Requête de reception de fichier envoyée pour '%s'\n", filename);
    // Création de la socket TCP du client
    int dSTCP = socket(PF_INET, SOCK_STREAM, 0);
    if (dSTCP == -1) {
        perror("❌ Erreur de création de la socket TCP");
        fclose(file);
        pthread_mutex_unlock(&file_mutex);
        printf("Unlock file\n");
        return;
    }
    printf("Socket TCP créée\n");
    printf("En attente du port TCP du serveur...\n");

    // Configuration pour recevoir le port TCP du serveur
    pthread_mutex_lock(&opcode_mutex);
    expected_opcode = 5;  // OpCode pour le message de port TCP
    msg_received = 0;
    pthread_mutex_unlock(&opcode_mutex);
        
    // Attente du message avec le port TCP
    int timeout_counter = 0;
    while (!msg_received && timeout_counter < 50) {  // attendre max 5 secondes
        usleep(100000);  // 100ms
        timeout_counter++;
    }
        
    pthread_mutex_lock(&opcode_mutex);
    if (!msg_received) {
        perror("❌ Timeout - Pas reçu de port TCP");
        expected_opcode = 0;
        pthread_mutex_unlock(&opcode_mutex);
        close(dSTCP);
        fclose(file);
    }
        
    // Récupération du port
    int port_tcp = ntohs(expected_msg.port);
    
    if (port_tcp == 0) {
        perror("❌ Erreur de conversion du port TCP");
        expected_opcode = 0;
        pthread_mutex_unlock(&opcode_mutex);
        close(dSTCP);
        fclose(file);
    }
    
    expected_opcode = 0;  // Reset pour les futurs messages
    msg_received = 0;
    pthread_mutex_unlock(&opcode_mutex);
        
    printf("Port TCP reçu : %d\n", htons(port_tcp));
        
    struct sockaddr_in tcpServerAddr = aS;
    tcpServerAddr.sin_port = htons(port_tcp);

    if (connect(dSTCP, (struct sockaddr*)&tcpServerAddr, sizeof(tcpServerAddr)) == -1) {
        perror("❌ Erreur de connexion TCP au serveur");
        close(dSTCP);
        fclose(file);
        pthread_mutex_unlock(&file_mutex);
        printf("Unlock file\n");
    }
    printf("Connexion à la socket TCP établie\n");

    char buffer[MAX_MSG_LEN];
    ssize_t bytesReceived;
    size_t totalBytesReceived = 0;
    
    while ((bytesReceived = recv(dSTCP, buffer, MAX_MSG_LEN, 0)) > 0) {
        if (fwrite(buffer, 1, bytesReceived, file) != (size_t)bytesReceived) {
            perror("Erreur lors de l'écriture dans le fichier");
            close(dSTCP);
            fclose(file);
            pthread_mutex_unlock(&file_mutex);
        }
        totalBytesReceived += bytesReceived;
    }
    
    fclose(file);
    close(dSTCP);
    pthread_mutex_unlock(&file_mutex);
    printf("Fichier reçu (%ld octets).\n", totalBytesReceived);
}


void upload_file_wrapper(void* args) {
    struct UploadArgs* ctx = (struct UploadArgs*) args;
    upload_file(ctx->filename, ctx->username, ctx->dS, ctx->aS, ctx->aD);
}
void download_file_wrapper(void* args) {
    struct DownloadArgs* ctx = (struct DownloadArgs*) args;
    download_file(ctx->filename, ctx->username, ctx->dS, ctx->aS, ctx->aD);
}