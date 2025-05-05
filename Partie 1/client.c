#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

struct msgBuffer {
    char username[32];
    int opCode;
    char msg[256];
    int msgSize;
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
    aD.sin_port = htons(atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &aD.sin_addr) <= 0) {
        perror("❌ Erreur inet_pton");
        close(dS);
        exit(EXIT_FAILURE);
    }
    socklen_t lgA = sizeof(struct sockaddr_in);
    printf("📡 Adresse IP et port configurés\n");

    struct msgBuffer m;
    strncpy(m.username, "Alice", sizeof(m.username));
    m.opCode = 1;
    strncpy(m.msg, "Bonjour serveur UDP", sizeof(m.msg));
    m.msgSize = strlen(m.msg);

    printf("📤 Envoi du message au serveur...\n");
    if (sendto(dS, &m, sizeof(m), 0, (struct sockaddr*)&aD, lgA) == -1) {
        perror("❌ Erreur sendto");
        close(dS);
        exit(EXIT_FAILURE);
    }
    printf("✅ Message envoyé. En attente de réponse...\n");

    int r;
    if (recvfrom(dS, &r, sizeof(int), 0, NULL, NULL) == -1) {
        perror("❌ Erreur recvfrom");
        close(dS);
        exit(EXIT_FAILURE);
    }

    printf("📥 Réponse reçue du serveur : %d\n", r);

    close(dS);
    printf("🔒 Socket fermée. Fin du client.\n");
    return 0;
}
