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


struct ThreadContext {
    int dS;
    struct sockaddr_in aS;
    char username[MAX_USERNAME_LEN];
};

void upload_file(char *filename, char* username, int dS, struct sockaddr_in aS, struct sockaddr_in aD) {
    bool fileExists = true;
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("❌ Erreur d'ouverture du fichier");
        printf("Le fichier '%s' n'existe pas ou n'est pas accessible\n", filename);
        fileExists = false;
    }


    if (fileExists) {
        struct msgBuffer m;
        strcpy(m.username, username);
        m.opCode = 2; // OpCode pour l'envoi de fichier
        m.port = htons(aD.sin_port);
        m.adClient = aD;
        strcpy(m.msg, filename);

        //Demande d'envoi de fichier
        if (sendto(dS, &m, sizeof(m), 0, (struct sockaddr*)&aS, sizeof(aS)) == -1) {
            perror("❌ Erreur d'envoi de la requête");
            fclose(file);
        }
        printf("📤 Requête d'envoi de fichier envoyée pour '%s'\n", filename);
        
        
        
        // Création de la socket TCP du client
        int dSTCP = socket(PF_INET, SOCK_STREAM, 0);
        if (dSTCP == -1) {
            perror("❌ Erreur de création de la socket TCP");
            fclose(file);
        }
        
        
        aSU.sin_port = htons(12347); // Port serveur fixe
        sleep(1);

        if (connect(dSTCP, (struct sockaddr*)&aS, sizeof(aS)) == -1) {
            perror("❌ Erreur de connexion à la socket TCP du serveur");
            close(dSTCP);
            fclose(file);
        }
        printf("Connexion à la socket TCP établie\n");
        
        
        
        // Envoi du fichier via TCP
        ssize_t bytesRead;
        char buffer[MAX_MSG_LEN];
        while ((bytesRead = fread(buffer, 1, MAX_MSG_LEN, file)) > 0) {
            if (send(dSTCP, buffer, bytesRead, 0) < 0) {
                perror("Erreur lors de l'envoi des données");
                close(dSTCP);
                fclose(file);
            }
            printf("Envoi de %ld octets au serveur.", bytesRead);
        }
    
        close(dSTCP);

    }
}

void download_file(char* filename, char* username, int dS, struct sockaddr_in aS, struct sockaddr_in aD) {
    FILE* file = fopen(filename, "wb");
    
    struct msgBuffer m;
        strcpy(m.username, username);
        m.opCode = 6; // OpCode pour l'envoi de fichier
        m.port = htons(aD.sin_port);
        m.adClient = aD;
        strcpy(m.msg, filename);

        //Demande de reception du fichier 
        if (sendto(dS, &m, sizeof(m), 0, (struct sockaddr*)&aS, sizeof(aS)) == -1) {
            perror("❌ Erreur d'envoi de la requête");
            fclose(file);
        }
        printf("📤 Requête de reception de fichier envoyée pour '%s'\n", filename);
        

        // Création de la socket TCP du client
        int dSTCP = socket(PF_INET, SOCK_STREAM, 0);
        if (dSTCP == -1) {
            perror("❌ Erreur de création de la socket TCP");
            fclose(file);
        }
        sleep(1);

        if (connect(dSTCP, (struct sockaddr*)&aS, sizeof(aS)) == -1) {
            perror("❌ Erreur de connexion à la socket TCP du serveur");
            close(dSTCP);
            fclose(file);
        }
        printf("Connexion à la socket TCP établie\n");

        char buffer[MAX_MSG_LEN];
        ssize_t bytesReceived;
        while ((bytesReceived = recv(dSTCP, buffer, MAX_MSG_LEN, 0)) > 0) {
            if (fwrite(buffer, 1, bytesReceived, file) != (size_t)bytesReceived) {
                perror("Erreur lors de l'écriture dans le fichier");
                close(dSTCP);
            }
        }
        fclose(file);
        printf("Fichier reçu.");
        close(dSTCP);

}

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
        scanf(" %[^\n]", m.msg);


        m.msgSize = strlen(m.msg) + 1;

        
        if (strncmp(m.msg, "@UPLOAD ", 8) == 0 && strlen(m.msg) > 8) {
            char *filename = m.msg + 8;
            // Bloquer la réception dans ce thread pour éviter la concurrence sur la socket UDP
            upload_file(filename, ctx->username, ctx->dS, ctx->aS, ctx->aS);
        }

        if (strncmp(m.msg, "@DOWNLOAD ", 10) == 0 && strlen(m.msg) > 10) {
            char *filename = m.msg + 10;
            // Bloquer la réception dans ce thread pour éviter la concurrence sur la socket UDP
            download_file(filename, ctx->username, ctx->dS, ctx->aS, ctx->aS);
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
    aS.sin_addr.s_addr = INADDR_ANY;

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
