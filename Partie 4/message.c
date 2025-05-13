#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "client_list.h"
#include "message.h"

// ✅ Envoie un message à un seul client
int sendMessageToOneClient(struct client client, struct msgBuffer* msg, int serverSocket) {
    socklen_t addrLen = sizeof(struct sockaddr_in);

    int result = sendto(
        serverSocket,
        msg,
        sizeof(struct msgBuffer),
        0,
        (struct sockaddr*)&client.adClient,
        addrLen
    );

    if (result == -1) {
        perror("❌ Erreur lors de l'envoi du message au client");
        return -1;
    } else {
        printf("📤 Message envoyé à %s [%s:%d]\n",
               client.username,
               inet_ntoa(client.adClient.sin_addr),
               ntohs(client.adClient.sin_port));
        return 0;
    }
}

// ✅ Envoie un message à tous les clients SAUF l'émetteur
int sendMessageToAllClients(ClientNode* clientList, struct msgBuffer* msg, int serverSocket) {
    ClientNode* current = clientList;
    int successCount = 0;
    int errorCount = 0;

    while (current != NULL) {
        struct client cible = current->data;

        if (!(cible.adClient.sin_addr.s_addr == msg->adClient.sin_addr.s_addr &&
              cible.adClient.sin_port == msg->adClient.sin_port)) {

            int result = sendMessageToOneClient(cible, msg, serverSocket);

            if (result == -1) {
                errorCount++;
            } else {
                successCount++;
            }
        }

        current = current->next;
    }

    printf("📢 Message diffusé à %d clients (%d erreurs)\n", successCount, errorCount);
    return successCount;
}

void envoyerMessageAListe(ClientNode* clientList, struct msgBuffer* msg, int serverSocket, struct client* exception) {
    ClientNode* current = clientList;

    while (current != NULL) {
        struct client cible = current->data;

        // ❌ Skip si exception non NULL et correspond au client cible
        if (exception != NULL &&
            cible.adClient.sin_addr.s_addr == exception->adClient.sin_addr.s_addr &&
            cible.adClient.sin_port == exception->adClient.sin_port) {
            current = current->next;
            continue;
        }

        sendMessageToOneClient(cible, msg, serverSocket);
        current = current->next;
    }
}

