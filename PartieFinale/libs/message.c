#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "common.h"
#include "client_list.h"
#include "message.h"
#include "server_com.h"
#include "server_mutex.h"

int sendMessageToOneClient(struct client client, struct msgBuffer* msg, int serverSocket) {
    socklen_t addrLen = sizeof(struct sockaddr_in);
    pthread_mutex_lock(&client_list_mutex);
    pthread_mutex_lock(&udp_socket_mutex);
    int result = sendto(
        serverSocket,
        msg,
        sizeof(struct msgBuffer),
        0,
        (struct sockaddr*)&client.adClient,
        addrLen
    );
    pthread_mutex_unlock(&udp_socket_mutex);
    pthread_mutex_unlock(&client_list_mutex);

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

int sendMessageToClient(struct msgBuffer* msg, int serverSocket, struct sockaddr_in* adClient) {
    socklen_t addrLen = sizeof(struct sockaddr_in);

    int result = sendto(serverSocket, msg, sizeof(struct msgBuffer), 0, (struct sockaddr*)adClient, addrLen
    );

    if (result == -1) {
        perror("❌ Erreur lors de l'envoi du message au client");
        return -1;
    } else {
        printf("📤 Message envoyé à l'adresse %s:%d\n",
               inet_ntoa(adClient->sin_addr),
               ntohs(adClient->sin_port));
        return 0;
    }
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

