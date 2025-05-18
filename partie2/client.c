#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>

#define MAX_USERNAME_LEN 50
#define MAX_MSG_LEN 512
#define MAX_PASSWORD_LEN 50

struct msgBuffer {
    char username[MAX_USERNAME_LEN];
    int opCode;
    char msg[MAX_MSG_LEN];
    int msgSize;
    int port;
    struct sockaddr_in adClient;
    char password[MAX_PASSWORD_LEN];
};

struct ThreadContext {
    int dS;
    struct sockaddr_in aS;
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
};

// Fonction pour envoyer des messages
void* send_thread(void* arg) {
    struct ThreadContext* ctx = (struct ThreadContext*) arg;
    struct msgBuffer m;
    strcpy(m.username, ctx->username);
    m.opCode = 1;
    m.port = ntohs(ctx->aS.sin_port);  // On convertit en port lisible
    m.adClient = ctx->aS;

    while (1) {
        printf("✉️ Entrez un message : ");
        scanf(" %[^\n]", m.msg); // Permet de lire les espaces
        m.msgSize = strlen(m.msg) + 1;

        if (sendto(ctx->dS, &m, sizeof(m), 0, (struct sockaddr*)&ctx->aS, sizeof(ctx->aS)) == -1) {
            perror("❌ Erreur sendto");
            break;
        }
        printf("📤 Message envoyé : %s\n", m.msg);
    }
    return NULL;
}

// Fonction pour recevoir des messages
void* recv_thread(void* arg) {
    struct ThreadContext* ctx = (struct ThreadContext*) arg;
    struct msgBuffer m;
    struct sockaddr_in from;
    socklen_t fromLen = sizeof(from);

    while (1) {
        if (recvfrom(ctx->dS, &m, sizeof(m), 0, (struct sockaddr*)&from, &fromLen) == -1) {
            perror("❌ Erreur recvfrom");
            break;
        }
        printf("\n📨 Message reçu de %s : %s\n", m.username, m.msg);
    }
    return NULL;
}

int main() {
    int dS = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in adServeur;
    adServeur.sin_family = AF_INET;
    adServeur.sin_port = htons(12345);
    inet_pton(AF_INET, "127.0.0.1", &adServeur.sin_addr);

    printf("📢 Bienvenue dans notre messagerie !\n");
    printf("📢 Utilisez la commande @help pour plus d'informations.\n");

    struct msgBuffer msg;
    memset(&msg, 0, sizeof(msg));
    msg.port = 12345;

    // Authentification
    printf("Pseudo: ");
    fgets(msg.username, MAX_USERNAME_LEN, stdin);
    msg.username[strcspn(msg.username, "\n")] = 0;
    printf("Mot de passe: ");
    fgets(msg.password, MAX_PASSWORD_LEN, stdin);
    msg.password[strcspn(msg.password, "\n")] = 0;
    snprintf(msg.msg, MAX_MSG_LEN, "@connect %s %s", msg.username, msg.password);
    sendto(dS, &msg, sizeof(msg), 0, (struct sockaddr*)&adServeur, sizeof(adServeur));

    printf("Connexion envoyée, en attente de la réponse du serveur...\n");

    // Lancer le thread de réception
    pthread_t th;
    pthread_create(&th, NULL, recv_thread, &dS);

    printf("Vous êtes connecté !\n");
    printf("Entrez une commande ou un message :\n");

    while (1) {
        fgets(msg.msg, MAX_MSG_LEN, stdin);
        msg.msg[strcspn(msg.msg, "\n")] = 0;
        sendto(dS, &msg, sizeof(msg), 0, (struct sockaddr*)&adServeur, sizeof(adServeur));
    }
    close(dS);
    return 0;
}
