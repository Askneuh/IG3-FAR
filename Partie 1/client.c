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

pthread_mutex_t udp_socket_mutex;
pthread_mutex_t file_mutex;
pthread_mutex_t recv_mutex;
volatile block_recv = 0; 

pthread_mutex_t opcode_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile int expected_opcode = 0;  // 0 = pas de blocage spécifique
struct msgBuffer expected_msg;     // Pour stocker le message attendu
volatile int msg_received = 0;     // Flag pour indiquer si le message a été reçu
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
    struct sockaddr_in aD;
    char username[MAX_USERNAME_LEN];
};

struct UploadArgs {
    char filename[MAX_MSG_LEN];
    char username[MAX_USERNAME_LEN];
    int dS;
    struct sockaddr_in aS;
    struct sockaddr_in aD;
};

struct DownloadArgs {
    char filename[MAX_MSG_LEN];
    char username[MAX_USERNAME_LEN];
    int dS;
    struct sockaddr_in aS;
    struct sockaddr_in aD;
};

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



int main(int argc, char *argv[]) {

    pthread_mutex_init(&udp_socket_mutex, 0);
    pthread_mutex_init(&file_mutex, 0);
    pthread_mutex_init(&recv_mutex, 0);
    pthread_mutex_init(&opcode_mutex, 0);

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

    struct ThreadContext ctx;
    ctx.dS = dS;
    ctx.aS = aS;
    ctx.aD = aD;
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
    pthread_mutex_destroy(&udp_socket_mutex);
    pthread_mutex_destroy(&file_mutex);
    pthread_mutex_destroy(&recv_mutex);
    pthread_mutex_destroy(&opcode_mutex);

    return 0;
}
