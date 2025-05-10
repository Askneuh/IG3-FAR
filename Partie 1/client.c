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



int main(int argc, char *argv[]) {
    printf("🟢 Démarrage du client UDP\n");

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <IP> <port>\n", argv[0]);
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

    // Liaison de la socket
    if (bind(dS, (struct sockaddr*)&aD, sizeof(aD)) < 0) {
        perror("❌ Erreur bind");
        exit(EXIT_FAILURE);
    }

    //adresse du serveur
    struct sockaddr_in aS;
    aS.sin_family = AF_INET;
    aS.sin_port = htons((short)12345);
    aS.sin_addr.s_addr = INADDR_ANY;


    struct msgBuffer m;

    printf("Entrez votre nom d'utilisateur : ");
    scanf("%s", m.username);
    m.opCode = 1;
    m.port = aD.sin_port;
    m.msgSize = strlen(m.msg) + 1;
    m.adClient = aD;

    while(1) {
        printf("Entrez un message à envoyer : ");
        scanf("%s", m.msg);

        if (sendto(dS, &m, sizeof(m), 0, (struct sockaddr*)&aS, sizeof(aS)) == -1) {
            perror("❌ Erreur sendto");
            close(dS);
        }
        printf("📤 Message envoyé au serveur : %s\n", m.msg);
    }
    
    close(dS);
    printf("🔒 Socket fermée. Fin du client.\n");
    return 0;
}
