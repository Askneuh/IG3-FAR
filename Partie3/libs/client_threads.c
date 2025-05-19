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
#include "client_threads.h"
#include "file_handler.h"
#include "client_mutex.h"
#include "variables.h"



// Fonction pour envoyer des messages
void* send_thread(void* arg) {
    struct ThreadContext* ctx = (struct ThreadContext*) arg;
    struct msgBuffer m;
    strcpy(m.username, ctx->username);
    m.opCode = 1;
    m.port = htons(ctx->aS.sin_port);  // On convertit en port lisible
    m.adClient = ctx->aS;

    bool continueEnvoie = true;

    while (continueEnvoie) {
        printf("\n✉️ Entrez un message : ");
        scanf(" %[^\n]", m.msg);

        m.msgSize = strlen(m.msg) + 1;
        
        if (strncmp(m.msg, "@UPLOAD ", 8) == 0 && strlen(m.msg) > 8) {
            char *filename = m.msg + 8;
            struct UploadArgs argsUpload;
            strcpy(argsUpload.filename, filename);
            argsUpload.dS = ctx->dS;
            argsUpload.aS = ctx->aS;
            argsUpload.aD = ctx->aD;
            strcpy(argsUpload.username, ctx->username);
            pthread_t upload_thread;
            pthread_create(&upload_thread, NULL, &upload_file_wrapper, &argsUpload);
        }
        else if (strncmp(m.msg, "@DOWNLOAD ", 10) == 0 && strlen(m.msg) > 10) {
            char *filename = m.msg + 10;
            struct DownloadArgs argsDownload;
            strcpy(argsDownload.filename, filename);
            argsDownload.dS = ctx->dS;
            argsDownload.aS = ctx->aS;
            argsDownload.aD = ctx->aD;
            
            strcpy(argsDownload.username, ctx->username);
            pthread_t download_thread;
            pthread_create(&download_thread, NULL, &download_file_wrapper, &argsDownload);
        } 
        else {
            pthread_mutex_lock(&udp_socket_mutex);
            printf("Lock UDP\n");
            if (sendto(ctx->dS, &m, sizeof(m), 0, (struct sockaddr*)&ctx->aS, sizeof(ctx->aS)) == -1) {
                perror("❌ Erreur sendto");
                continueEnvoie = false; 
            }
            pthread_mutex_unlock(&udp_socket_mutex);
            printf("Unlock UDP\n");
        }
    }
    
}


// Fonction pour recevoir des messages
void* recv_thread(void* arg) {
    struct ThreadContext* ctx = (struct ThreadContext*) arg;
    struct msgBuffer m;
    struct sockaddr_in from;
    socklen_t fromLen = sizeof(from);
    bool continueReception = true;
    
    while (continueReception) {
        // Si block_recv est actif, attendre un peu et continuer
        if (block_recv) {
            usleep(100000); // pause de 100ms
            continue;
        }
        
        if (recvfrom(ctx->dS, &m, sizeof(m), 0, (struct sockaddr*)&from, &fromLen) == -1) {
            perror("❌ Erreur recvfrom");
            continueReception = false;
        }
        // Vérifier si ce message correspond à un opcode attendu par un autre thread
        pthread_mutex_lock(&opcode_mutex);
        if (expected_opcode > 0 && m.opCode == expected_opcode) {
            // C'est un message attendu par un autre thread, le stocker
            expected_msg = m;
            msg_received = 1;
            pthread_mutex_unlock(&opcode_mutex);
            continue;  // Ne pas l'afficher, continuer la boucle
        }
        pthread_mutex_unlock(&opcode_mutex);
        printf("\n📨 Message reçu de %s : %s\n", m.username, m.msg);
    }
    
    
}
