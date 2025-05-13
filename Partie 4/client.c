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

struct msgBuffer {
    char username[MAX_USERNAME_LEN];
    int opCode;
    char msg[MAX_MSG_LEN];
    int msgSize;
    int port;
    struct sockaddr_in adClient;
};

struct ThreadContext {
    int dS;
    struct sockaddr_in aS;
    char username[MAX_USERNAME_LEN];
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
        if (m.opCode == 1) {
            printf("\n📨 Message reçu de %s : %s\n", m.username, m.msg);
        }
        else if (m.opCode == 3){
            printf("\n notification serveur : %s\n", m.msg);

        }
        
    }
    return NULL;
}

int main(int argc, char *argv[]) {
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
    aS.sin_port = htons((short)12345); // Port serveur fixe
    aS.sin_addr.s_addr = inet_addr(argv[1]); // IP serveur fournie

    struct ThreadContext ctx;
    ctx.dS = dS;
    ctx.aS = aS;

    printf("Entrez votre nom d'utilisateur : ");
    scanf("%s", ctx.username);

    // Création des threads
    pthread_t sendT, recvT;
    pthread_create(&sendT, NULL, send_thread, &ctx);
    pthread_create(&recvT, NULL, recv_thread, &ctx);

    pthread_join(sendT, NULL);
    pthread_join(recvT, NULL);

    close(dS);
    printf("🔒 Socket fermée. Fin du client.\n");
    return 0;
}
