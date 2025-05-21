#include "commandes.h"
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

// Envoi formaté à un client (inchangé)
void sendToClient(const char* sender, const char* message, struct sockaddr_in addr, int dS) {
    struct msgBuffer m;
    memset(&m, 0, sizeof(m));
    strncpy(m.username, sender, MAX_USERNAME_LEN);
    strncpy(m.msg, message, MAX_MSG_LEN);
    m.opCode = 1;
    m.adClient = addr;

    sendto(dS, &m, sizeof(m), 0, (struct sockaddr*)&addr, sizeof(addr));
}

// Diffusion à tous sauf l'expéditeur (utilisé pour @quit, @rename)
void sendAllExceptSender(ClientNode* list, const char* message, struct sockaddr_in senderAddr, int dS) {
    while (list) {
        if (list->client.adClient.sin_addr.s_addr != senderAddr.sin_addr.s_addr ||
            list->client.adClient.sin_port != senderAddr.sin_port) {
            sendToClient("Broadcast", message, list->client.adClient, dS);
        }
        list = list->next;
    }
}

// Diffusion à tous, y compris l'expéditeur (utilisé pour @shutdown)
void sendAllToEveryone(ClientNode* list, const char* message, int dS) {
    while (list) {
        sendToClient("Broadcast", message, list->client.adClient, dS);
        list = list->next;
    }
}
// Appel standard : diffuse à tous sauf l'expéditeur
void sendAll(ClientNode* list, const char* message, struct sockaddr_in senderAddr, int dS) {
    sendAllExceptSender(list, message, senderAddr, dS);
}



// Affichage de l'aide depuis un fichier
void sendHelp(struct sockaddr_in clientAddr, int dS) {
    FILE *f = fopen("help.txt", "r");
    if (!f) {
        sendToClient("Serveur", "❌ Fichier d'aide introuvable", clientAddr, dS);
        return;
    }

    char line[MAX_MSG_LEN];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';

        char finalLine[MAX_MSG_LEN];
        snprintf(finalLine, sizeof(finalLine), "\033[36m[INFO]\033[0m %s", line);
        sendToClient("Aide", finalLine, clientAddr, dS);
    }

    fclose(f);
}

// Envoi des crédits depuis un fichier
void sendCredits(struct sockaddr_in clientAddr, int dS) {
    FILE* f = fopen("credits.txt", "r");
    if (!f) {
        sendToClient("Serveur", "❌ Fichier de crédits introuvable", clientAddr, dS);
        return;
    }

    char line[MAX_MSG_LEN];
    while (fgets(line, sizeof(line), f)) {
        sendToClient("Crédits", line, clientAddr, dS);
    }

    fclose(f);
}

// Liste des clients connectés
void sendList(ClientNode* list, struct sockaddr_in clientAddr, int dS) {
    char buffer[1024] = "Clients connectés : ";
    size_t len = strlen(buffer);

    while (list) {
        strncat(buffer, list->client.username, sizeof(buffer) - len - 1);
        len = strlen(buffer);
        strncat(buffer, ", ", sizeof(buffer) - len - 1);
        list = list->next;
    }

    if (len > 2 && buffer[len - 2] == ',') {
        buffer[len - 2] = '\0';
    }

    sendToClient("Serveur", buffer, clientAddr, dS);  // Envoi uniquement à l'expéditeur
}


// Message privé
void sendPrivateMessage(ClientNode* list, struct msgBuffer* msg, int dS) {
    char* target = strtok(msg->msg + 4, " ");
    char* content = strtok(NULL, "");

    if (!target || !content) {
        sendToClient("Serveur", "❌ Usage: @mp <pseudo> <message>", msg->adClient, dS);
        return;
    }

    ClientNode* dest = list;
    while (dest) {
        if (strcmp(dest->client.username, target) == 0) {
            sendToClient(msg->username, content, dest->client.adClient, dS);  // Envoi au destinataire
            sendToClient("Serveur", "📤 Message privé envoyé", msg->adClient, dS);  // Confirmation à l’expéditeur
            return;
        }
        dest = dest->next;
    }

    sendToClient("Serveur", "❌ Destinataire introuvable", msg->adClient, dS);
}

// Renommer un utilisateur
void renameUser(ClientNode* list, struct msgBuffer* msg, int dS) {
    char* newName = msg->msg + 8;
    while (*newName == ' ') newName++;

    if (strlen(newName) == 0 || strlen(newName) >= MAX_USERNAME_LEN) {
        sendToClient("Serveur", "❌ Pseudo invalide", msg->adClient, dS);
        return;
    }

    ClientNode* current = list;
    while (current) {
        if (current->client.adClient.sin_addr.s_addr == msg->adClient.sin_addr.s_addr &&
            current->client.adClient.sin_port == msg->adClient.sin_port) {
            strncpy(current->client.username, newName, MAX_USERNAME_LEN);
            char notif[MAX_MSG_LEN];
            snprintf(notif, sizeof(notif), "🔁 %s a changé son pseudo en %s", msg->username, newName);
            sendAll(list, notif, msg->adClient, dS); // Envoie à tous sauf l’expéditeur
            sendToClient("Serveur", "✅ Pseudo changé avec succès", msg->adClient, dS); // Confirmation
            return;
        }
        current = current->next;
    }

    sendToClient("Serveur", "❌ Client non trouvé", msg->adClient, dS);
}

// Déconnexion d'un client
void sendQuit(ClientNode** clientList, struct msgBuffer* msg, int dS) {
    char notif[MAX_MSG_LEN];
    snprintf(notif, sizeof(notif), "❌ %s s’est déconnecté du serveur", msg->username);
    sendAll(*clientList, notif, msg->adClient, dS);

    char quitMsg[MAX_MSG_LEN];
    snprintf(quitMsg, sizeof(quitMsg), "\033[36m[INFO]\033[0m Vous avez été déconnecté du serveur, %s.", msg->username);
    sendToClient("Serveur", quitMsg, msg->adClient, dS);

    *clientList = removeClient(*clientList, msg->username);

    printf("\033[36m[INFO]\033[0m Le client %s s'est déconnecté\n", msg->username);
}

// Vérification des identifiants
bool verifyCredentials(const char* username, const char* password) {
    FILE* file = fopen("users.txt", "r");
    if (!file) return false;

    char storedUser[50], storedPass[50];
    while (fscanf(file, "%s %s", storedUser, storedPass) == 2) {
        if (strcmp(username, storedUser) == 0 && strcmp(password, storedPass) == 0) {
            fclose(file);
            return true;
        }
    }
    fclose(file);
    return false;
}

// Vérifie si le message est une commande spécifique
int isCmd(const char* msg, const char* cmd) {
    size_t len = strlen(cmd);
    return strncmp(msg, cmd, len) == 0 && (msg[len] == '\0' || msg[len] == ' ');
}

// Traitement central des commandes
ClientNode* traiterCommande(struct msgBuffer* msg, ClientNode* list, int dS) {
    printf("Debug: message reçu = '%s'\n", msg->msg);

    // SUPPRIME ce bloc :
    // if (isCmd(msg->msg, "@connect")) { ... }

    if (isCmd(msg->msg, "@help")) {
        sendHelp(msg->adClient, dS);
    } else if (isCmd(msg->msg, "@credits")) {
        sendCredits(msg->adClient, dS);
    } else if (isCmd(msg->msg, "@ping")) {
        sendToClient("Serveur", "pong", msg->adClient, dS);
    } else if (isCmd(msg->msg, "@list")) {
        sendList(list, msg->adClient, dS);
    } else if (isCmd(msg->msg, "@rename")) {
        renameUser(list, msg, dS);
    } else if (isCmd(msg->msg, "@mp")) {
        sendPrivateMessage(list, msg, dS);
    } else if (isCmd(msg->msg, "@quit")) {
        sendQuit(&list, msg, dS);
    } else if (isCmd(msg->msg, "@shutdown")) {
        sendAllToEveryone(list, "⚠️ Le serveur va s'arrêter", dS);
        kill(getpid(), SIGINT);
    } else {
        sendToClient("Serveur", "❌ Commande inconnue", msg->adClient, dS);
    }
    return list;
}
