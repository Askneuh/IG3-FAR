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
#include "variables.h"
#include "client_mutex.h"
#include "client_threads.h"
#include "file_handler.h"
#include <signal.h>


#define MAX_USERNAME_LEN 50
#define MAX_MSG_LEN 512
#define MAX_PASSWORD_LEN 50

typedef struct {
    int dS;
    struct sockaddr_in aS;
    char username[MAX_USERNAME_LEN];
    struct sockaddr_in aD;
} DisconnectInfo;

static DisconnectInfo* disconnect_info = NULL;

// Fonction pour gérer la déconnexion propre
void handle_disconnect(int sig) {
    if (disconnect_info != NULL) {
        struct msgBuffer msg;
        memset(&msg, 0, sizeof(msg));
        strcpy(msg.username, disconnect_info->username);
        strcpy(msg.msg, "@disconnect");
        msg.opCode = 10; // Code pour déconnexion
        msg.adClient = disconnect_info->aD;
        msg.port = disconnect_info->aD.sin_port;

        // Envoyer le message de déconnexion
        pthread_mutex_lock(&udp_socket_mutex);
        sendto(disconnect_info->dS, &msg, sizeof(msg), 0, 
              (struct sockaddr*)&disconnect_info->aS, sizeof(disconnect_info->aS));
        pthread_mutex_unlock(&udp_socket_mutex);

        printf("\nDéconnexion envoyée au serveur.\n");
    }
}

int main(int argc, char *argv[]) {
    init_opcode_mutex();
    init_mutexes();
    init_variables();

    printf("🟢 Démarrage du client UDP\n");

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <IP serveur> <port client>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    printf("🔧 Création de la socket...\n");
    int dS = socket(PF_INET, SOCK_DGRAM, 0);
    if (dS == -1) {
        perror("❌ Erreur socket");
        exit(EXIT_FAILURE);
    }
    printf("✅ Socket créée avec succès\n");


    struct sockaddr_in aD;
    aD.sin_family = AF_INET;
    aD.sin_addr.s_addr = inet_addr(argv[1]);
    aD.sin_port = htons((short)atoi(argv[2]));

    if (bind(dS, (struct sockaddr*)&aD, sizeof(aD)) < 0) {
        perror("❌ Erreur bind");
        close(dS);
        exit(EXIT_FAILURE);
    }

    // Adresse du serveur
    struct sockaddr_in aS;
    aS.sin_family = AF_INET;
    aS.sin_addr.s_addr = INADDR_ANY;
    aS.sin_port = htons((short)12345);

    // Préparer les infos pour la déconnexion
    DisconnectInfo info;
    info.dS = dS;
    info.aS = aS;
    info.aD = aD;
    memset(info.username, 0, MAX_USERNAME_LEN);
    disconnect_info = &info;

    // Configurer le gestionnaire de signal APRÈS avoir initialisé disconnect_info
    signal(SIGINT, handle_disconnect);


    struct ThreadContext ctx;
    ctx.dS = dS;
    ctx.aS = aS;
    ctx.aD = aD;
    memset(ctx.username, 0, MAX_USERNAME_LEN);

    struct msgBuffer msg;
    memset(&msg, 0, sizeof(msg));
    msg.port = 12345; //Port de qui ?

    // Authentification
    printf("Pseudo: ");
    fgets(msg.username, MAX_USERNAME_LEN, stdin);
    msg.username[strcspn(msg.username, "\n")] = 0;
    
    // Copier le nom d'utilisateur dans le contexte du thread
    strcpy(ctx.username, msg.username);
    
    printf("Mot de passe: ");
    fgets(msg.password, MAX_PASSWORD_LEN, stdin);
    msg.password[strcspn(msg.password, "\n")] = 0;
    snprintf(msg.msg, MAX_MSG_LEN, "@connect %s %s", msg.username, msg.password);
    msg.opCode = 9;
    msg.port = htons(aD.sin_port); 
    msg.adClient = aD;
    pthread_mutex_lock(&udp_socket_mutex);
    sendto(dS, &msg, sizeof(msg), 0, (struct sockaddr*)&aS, sizeof(aS));
    pthread_mutex_unlock(&udp_socket_mutex);
    printf("Connexion envoyée, en attente de la réponse du serveur...\n");
    

    // Création des threads
    pthread_t sendT, recvT;
    pthread_create(&sendT, NULL, send_thread, &ctx);
    pthread_create(&recvT, NULL, recv_thread, &ctx);

    pthread_join(sendT, NULL);
    pthread_join(recvT, NULL);

    close(dS);
    printf("🔒 Socket fermée. Fin du client.\n");
    destroy_mutexes();
    destroy_opcode_mutex();

    return 0;
}
