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

static bool is_typing = false;
static pthread_mutex_t typing_mutex = PTHREAD_MUTEX_INITIALIZER;

void* send_thread(void* arg) {
    struct ThreadContext* ctx = (struct ThreadContext*) arg;
    struct msgBuffer m;
    strcpy(m.username, ctx->username);
    m.opCode = 1;
    m.port = htons(ctx->aS.sin_port);
    m.adClient = ctx->aS;
    bool continueEnvoie = true;

    while (continueEnvoie) {
        pthread_mutex_lock(&typing_mutex);
        is_typing = true;
        pthread_mutex_unlock(&typing_mutex);
        
        printf("\n✉️ Entrez un message : ");
        fflush(stdout);  // Force l'affichage immédiat de l'invite
        
        if (fgets(m.msg, sizeof(m.msg), stdin) != NULL) {
            m.msg[strcspn(m.msg, "\n")] = 0;
        }
        
        pthread_mutex_lock(&typing_mutex);
        is_typing = false;
        pthread_mutex_unlock(&typing_mutex);

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
        else if (m.msg[0] == '@') {
           m.opCode = 8; 
           pthread_mutex_lock(&udp_socket_mutex);
           if (sendto(ctx->dS, &m, sizeof(m), 0, (struct sockaddr*)&ctx->aS, sizeof(ctx->aS)) == -1) {
               perror("❌ Erreur sendto");
               continueEnvoie = false;
           }
           pthread_mutex_unlock(&udp_socket_mutex);
       }
       else {
           m.opCode = 1 ;
           pthread_mutex_lock(&udp_socket_mutex);
           if (sendto(ctx->dS, &m, sizeof(m), 0, (struct sockaddr*)&ctx->aS, sizeof(ctx->aS)) == -1) {
               perror("❌ Erreur sendto");
               continueEnvoie = false;
           }
           pthread_mutex_unlock(&udp_socket_mutex);
       }
    }
    return NULL;
}

void* recv_thread(void* arg) {
    struct ThreadContext* ctx = (struct ThreadContext*) arg;
    struct msgBuffer m;
    struct sockaddr_in from;
    socklen_t fromLen = sizeof(from);
    bool continueReception = true;
    
    while (continueReception) {
        if (block_recv) {
            usleep(100000); // pause de 100ms
            continue;
        }
        
        if (recvfrom(ctx->dS, &m, sizeof(m), 0, (struct sockaddr*)&from, &fromLen) == -1) {
            perror("❌ Erreur recvfrom");
            continueReception = false;
            continue;
        }
        
        // Vérifier si ce message correspond à un opcode attendu par un autre thread
        pthread_mutex_lock(&opcode_mutex);
        if (expected_opcode > 0 && m.opCode == expected_opcode) {
            // C'est un message attendu par un autre thread, le stocker
            expected_msg = m;
            msg_received = 1;
            pthread_mutex_unlock(&opcode_mutex);
            continue; 
        }
        pthread_mutex_unlock(&opcode_mutex);
        
        // Gérer l'affichage proprement
        pthread_mutex_lock(&typing_mutex);
        bool currently_typing = is_typing;
        pthread_mutex_unlock(&typing_mutex);
        
        if (currently_typing) {
            // Si l'utilisateur tape, on fait une nouvelle ligne avant d'afficher
            printf("\n");
        }
        
        if (m.opCode == 1) {
            printf("📨 Message reçu de %s du chat commun : %s\n", m.username, m.msg);
        }
        else if (m.opCode == 8) {
            printf("📢 Notification serveur : %s\n", m.msg);
        }
        else if (m.opCode == 7){
            printf("📨 Message reçu de %s provenant de votre salon : %s\n", m.username, m.msg);
        }
        else if (m.opCode == 3) { 
            printf("%s : %s\n", m.username ,m.msg);
        }
        
        // Si l'utilisateur était en train de taper, réafficher l'invite
        if (currently_typing) {
            printf("✉️ Entrez un message : ");
            fflush(stdout);
        }
    }
}