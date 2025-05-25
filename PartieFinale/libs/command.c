#include "command.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "message.h"
#include "client_list.h"
#include "users.h"

static void cmd_help(struct msgBuffer* msg, int dS) {
    FILE* f = fopen("help.txt", "r");
    struct msgBuffer response;
    memset(&response, 0, sizeof(response));
    strcpy(response.username, "server");
    response.adClient = msg->adClient;
    response.port = msg->port;
    if (f) {
        fread(response.msg, 1, MAX_MSG_LEN-1, f);
        fclose(f);
    } else {
        strcpy(response.msg, "Aide indisponible.");
    }
    sendMessageToClient(&response, dS, &msg->adClient);
}

static void cmd_ping(struct msgBuffer* msg, int dS) {
    struct msgBuffer response;
    memset(&response, 0, sizeof(response));
    strcpy(response.username, "server");
    strcpy(response.msg, "pong");
    response.adClient = msg->adClient;
    response.port = msg->port;
    sendMessageToClient(&response, dS, &msg->adClient);
}
static void cmd_connect(struct msgBuffer* msg, int dS, User* users, int nbUsers, Command* cmd, ClientNode** clientList) {
    struct msgBuffer response;
    memset(&response, 0, sizeof(response));
    strcpy(response.username, "server");
    response.adClient = msg->adClient;
    response.port = msg->port;

    int idx = findUser(users, nbUsers, cmd->arg1);
    if (idx >= 0 && strcmp(users[idx].password, cmd->arg2) == 0) {
        strcpy(response.msg, "Connexion réussie !");
        // Ajouter le client s’il n’est pas déjà présent et pseudo valide
        if (
            users[idx].username[0] != '\0' &&
            strcmp(users[idx].username, "server") != 0 &&
            users[idx].username[0] >= 32 && users[idx].username[0] <= 126 &&
            !findClientByName(*clientList, users[idx].username)
        ) {
            struct client newClient;
            strcpy(newClient.username, users[idx].username);
            newClient.adClient = msg->adClient;  
            newClient.port = msg->port;
            newClient.dSClient = dS;
            *clientList = addClient(*clientList, newClient);
        }
    } else {
        strcpy(response.msg, "Identifiants invalides.");
    }

    sendMessageToClient(&response, dS, &msg->adClient);
}


static void cmd_credits(struct msgBuffer* msg, int dS) {
    FILE* f = fopen("credits.txt", "r");
    struct msgBuffer response;
    memset(&response, 0, sizeof(response));
    strcpy(response.username, "server");
    response.adClient = msg->adClient;
    response.port = msg->port;
    if (f) {
        fread(response.msg, 1, MAX_MSG_LEN-1, f);
        fclose(f);
    } else {
        strcpy(response.msg, "Crédits indisponibles.");
    }
    sendMessageToClient(&response, dS, &msg->adClient);
}

static void cmd_shutdown(struct msgBuffer* msg, int dS, User* users, int nbUsers) {
    struct msgBuffer response;
    memset(&response, 0, sizeof(response));
    strcpy(response.username, "server");
    response.adClient = msg->adClient;
    response.port = msg->port;
    // Vérifier si l'utilisateur est admin
    int idx = findUser(users, nbUsers, msg->username);
    if (idx >= 0 && users[idx].is_admin) {
        strcpy(response.msg, "Serveur en cours d'arrêt...");
        sendMessageToClient(&response, dS, &msg->adClient);
        exit(0);
    } else {
        strcpy(response.msg, "Commande réservée à l'admin.");
        sendMessageToClient(&response, dS, &msg->adClient);
    }
}

static void cmd_msg(struct msgBuffer* msg, int dS, ClientNode* clientList, Command* cmd) {
    struct msgBuffer response;
    memset(&response, 0, sizeof(response));
    strcpy(response.username, "server");
    response.adClient = msg->adClient;
    response.port = msg->port;
    ClientNode* dest = findClientByName(clientList, cmd->arg1);
    if (dest) {
        snprintf(response.msg, MAX_MSG_LEN, "[privé de %s]: %s", msg->username, cmd->arg2);
        sendMessageToClient(&response, dS, &dest->data.adClient);
        snprintf(response.msg, MAX_MSG_LEN, "Message privé envoyé à %s.", cmd->arg1);
    } else {
        snprintf(response.msg, MAX_MSG_LEN, "Utilisateur %s introuvable.", cmd->arg1);
    }
    sendMessageToClient(&response, dS, &msg->adClient);
}

static void cmd_list(struct msgBuffer* msg, int dS, ClientNode* clientList) {
    struct msgBuffer response = {0};
    strcpy(response.username, "server");
    response.adClient = msg->adClient;
    response.port = msg->port;

    char liste[MAX_MSG_LEN] = "Utilisateurs connectés :";
    ClientNode* cur = clientList;
    while (cur != NULL) {
        if (cur->data.username[0] != '\0' &&
            strcmp(cur->data.username, "server") != 0 &&
            cur->data.username[0] >= 32 && cur->data.username[0] <= 126) {
            char tmp[128];
            snprintf(tmp, sizeof(tmp), " %s(%d)", cur->data.username, cur->data.port);
            strcat(liste, tmp);
        }
        cur = cur->next;
    }
    snprintf(response.msg, MAX_MSG_LEN, "%s", liste);
    sendMessageToClient(&response, dS, &msg->adClient);
}


void parseCommand(const char* input, Command* cmd) {
    memset(cmd, 0, sizeof(Command));
    if (strncmp(input, "@help", 5) == 0) {
        cmd->type = CMD_HELP;
    } else if (strncmp(input, "@ping", 5) == 0) {
        cmd->type = CMD_PING;
    } else if (strncmp(input, "@credits", 8) == 0) {
        cmd->type = CMD_CREDITS;
    } else if (strncmp(input, "@shutdown", 9) == 0) {
        cmd->type = CMD_SHUTDOWN;
    } else if (strncmp(input, "@list", 5) == 0) {
        cmd->type = CMD_LIST;
    } else if (strncmp(input, "@msg", 4) == 0) {
        cmd->type = CMD_MSG;
        // Extraction des arguments : @msg user message
        const char* p = input + 4;
        while (*p == ' ') p++;
        sscanf(p, "%63s %255[^\n]", cmd->arg1, cmd->arg2);
    }else if (strncmp(input, "@connect", 8) == 0) {
        cmd->type = CMD_CONNECT;
        const char* p = input + 8;
        while (*p == ' ') p++;
        sscanf(p, "%63s %63s", cmd->arg1, cmd->arg2); // arg1 = login, arg2 = mdp
    }else {
        cmd->type = CMD_UNKNOWN;
    }
}

// Fonction centrale
void traiterCommande(Command* cmd, struct msgBuffer* msg, int dS, ClientNode** clientList, User* users, int nbUsers) {
    switch (cmd->type) {
        case CMD_HELP:
            cmd_help(msg, dS);
            break;
        case CMD_PING:
            cmd_ping(msg, dS);
            break;
        case CMD_CREDITS:
            cmd_credits(msg, dS);
            break;
        case CMD_SHUTDOWN:
            cmd_shutdown(msg, dS, users, nbUsers);
            break;
        case CMD_MSG:
            cmd_msg(msg, dS, *clientList, cmd);
            break;
        case CMD_LIST:
            cmd_list(msg, dS, *clientList);
            break;
        case CMD_CONNECT:
            cmd_connect(msg, dS, users, nbUsers, cmd, clientList);
            break;
        default:
            // Commande inconnue
            {
                struct msgBuffer response;
                memset(&response, 0, sizeof(response));
                strcpy(response.username, "server");
                strcpy(response.msg, "Commande inconnue.");
                response.adClient = msg->adClient;
                response.port = msg->port;
                sendMessageToClient(&response, dS, &msg->adClient);
            }
    }
}