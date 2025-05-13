#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>

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

struct fileBuffer {
    char filename[MAX_MSG_LEN];
    int fileSize;
    char fileData[MAX_MSG_LEN];
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
    m.port = htons(ctx->aS.sin_port);  // On convertit en port lisible
    m.adClient = ctx->aS;
    bool continueEnvoie = true;
    while (continueEnvoie) {
        printf("✉️ Entrez un message : ");
        scanf(" %[^\n]", m.msg); // Permet de lire les espaces
        m.msgSize = strlen(m.msg) + 1;
        // Détecter si le message commence par "@UPLOAD " suivi d'un nom de fichier
        if (strncmp(m.msg, "@UPLOAD ", 8) == 0 && strlen(m.msg) > 8) {
            char *filename = m.msg + 8;
            // Bloquer la réception dans ce thread pour éviter la concurrence sur la socket UDP
            upload_file(filename, ctx->username, ctx->dS, ctx->aS, ctx->aS);
        } 
        else {
            if (sendto(ctx->dS, &m, sizeof(m), 0, (struct sockaddr*)&ctx->aS, sizeof(ctx->aS)) == -1) {
                perror("❌ Erreur sendto");
                continueEnvoie = false; 
            }
            printf("📤 Message envoyé : %s\n", m.msg);
        }
    }
    return NULL;
}

// Fonction pour recevoir des messages
void* recv_thread(void* arg) {
    struct ThreadContext* ctx = (struct ThreadContext*) arg;
    struct msgBuffer m;
    struct sockaddr_in from;
    socklen_t fromLen = sizeof(from);
    bool continueReception = true;
    while (continueReception) {
        if (recvfrom(ctx->dS, &m, sizeof(m), 0, (struct sockaddr*)&from, &fromLen) == -1) {
            perror("❌ Erreur recvfrom");
            continueReception = false;
        }
        printf("\n📨 Message reçu de %s : %s\n", m.username, m.msg);
    }
    

}

void upload_file(char *filename, char* username, int dS, struct sockaddr_in aS, struct sockaddr_in aD) {
    // Vérification de l'existence du fichier avant d'envoyer la requête
    bool fileExists = true;
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("❌ Erreur d'ouverture du fichier");
        printf("Le fichier '%s' n'existe pas ou n'est pas accessible\n", filename);
        fileExists = false;
    }
    if (fileExists) {
        // Préparation du message de requête d'envoi de fichier
        struct msgBuffer m;
        strcpy(m.username, username);
        m.opCode = 2; // OpCode pour l'envoi de fichier
        m.port = htons(aD.sin_port); // Port du client
        m.adClient = aD;
        
        // Envoi de la requête de transfert de fichier
        if (sendto(dS, &m, sizeof(m), 0, (struct sockaddr*)&aS, sizeof(aS)) == -1) {
            perror("❌ Erreur d'envoi de la requête");
            fclose(file);
        }
        printf("📤 Requête d'envoi de fichier envoyée pour '%s'\n", filename);
        
        // Attente de la réponse du serveur contenant les infos de la socket TCP
        struct sockaddr_in from;
        socklen_t fromLen = sizeof(from);
        
        // Création de la socket TCP du client
        int dSTCP = socket(PF_INET, SOCK_STREAM, 0);
        if (dSTCP == -1) {
            perror("❌ Erreur de création de la socket TCP");
            fclose(file);
        }
        
        // Utilisation de l'adresse du serveur pour la connexion TCP
        struct sockaddr_in serverTCP;
        serverTCP.sin_family = AF_INET;
        serverTCP.sin_addr.s_addr = aS.sin_addr.s_addr;
        serverTCP.sin_port = htons(12346); // Port TCP du serveur
        printf("🔌 Adresse du serveur TCP : %s:%d\n", inet_ntoa(serverTCP.sin_addr), ntohs(serverTCP.sin_port));
        
        // Connexion à la socket TCP du serveur
        if (connect(dSTCP, (struct sockaddr*)&serverTCP, sizeof(serverTCP)) == -1) {
            perror("❌ Erreur de connexion à la socket TCP du serveur");
            close(dSTCP);
            fclose(file);
        }
        printf("🔗 Connexion à la socket TCP établie\n");
        
        struct fileBuffer f;
        strncpy(f.filename, filename, MAX_MSG_LEN - 1);
        f.filename[MAX_MSG_LEN - 1] = '\0'; // Garantir la terminaison de la chaîne
        
        // Envoi des données du fichier par morceaux
        size_t bytesRead;
        int totalSent = 0;
        while ((bytesRead = fread(f.fileData, 1, sizeof(f.fileData), file)) > 0) {
            f.fileSize = bytesRead;
            
            // Envoi de la structure fileBuffer
            ssize_t bytesSent = send(dSTCP, &f, sizeof(f.filename) + sizeof(f.fileSize) + bytesRead, 0);
            if (bytesSent == -1) {
                perror("❌ Erreur lors de l'envoi du fichier");
                close(dSTCP);
                fclose(file);
            }
            
            totalSent += bytesRead;
            printf("📤 Envoi en cours: %d octets envoyés\n", totalSent);
        }
        
        f.fileSize = 0;
        send(dSTCP, &f, sizeof(f.filename) + sizeof(f.fileSize), 0);
        
        close(dSTCP);
        fclose(file);
        printf("✅ Fichier '%s' envoyé avec succès (%d octets)\n", filename, totalSent);
    }
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
