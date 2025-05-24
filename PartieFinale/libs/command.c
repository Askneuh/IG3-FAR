#include "command.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "message.h"
#include "client_list.h"
#include "users.h"
#include "file_manager.h"

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
    FileManager* fm = open_file("users.txt", "a+"); // a+ pour lire ET écrire, créer si n'existe pas
    struct msgBuffer response;
    memset(&response, 0, sizeof(response));
    strcpy(response.username, "server");
    response.adClient = msg->adClient;
    response.port = msg->port;
    
    if (fm) {
        char* content = read_all(fm);
        if (content == NULL) {
            content = strdup("");
        }
        
        char *line = strtok(content, "\n");
        bool user_exists = false;
        bool password_correct = false;
        char username[MAX_USERNAME_LEN];
        char password[MAX_PASSWORD_LEN]; 
        
        // Parcourir le fichier pour chercher l'utilisateur
        while (line != NULL && !user_exists) {
            if (sscanf(line, "%49s %49s", username, password) == 2) { 
                if (strcmp(username, msg->username) == 0) {
                    user_exists = true;
                    if (strcmp(password, msg->password) == 0) {
                        password_correct = true;
                    }
                }
            }
            line = strtok(NULL, "\n");
        }
        
        if (user_exists) {
            if (password_correct) {
                strcpy(response.msg, "Connexion réussie"); 
                
                // Ajouter le client à la liste des connectés
                struct client newClient;
                strcpy(newClient.username, msg->username);
                newClient.adClient = msg->adClient;
                newClient.port = msg->port;
                newClient.dSClient = dS;
                
                // Vérifier qu'il n'est pas déjà dans la liste
                if (!clientAlreadyExists(*clientList, newClient)) {
                    *clientList = addClient(*clientList, newClient);
                    printf("✅ Client %s connecté\n", msg->username);
                }
            } else {
                strcpy(response.msg, "Mot de passe incorrect"); 
            }
        } else {
            close_file(fm); 
            fm = open_file("users.txt", "a");
            if (fm) {
                char new_user_line[256];
                snprintf(new_user_line, sizeof(new_user_line), "%s %s\n", msg->username, msg->password);
                write_line(fm, new_user_line); 
                
                strcpy(response.msg, "Utilisateur enregistré et connecté"); 
                
                struct client newClient;
                strcpy(newClient.username, msg->username);
                newClient.adClient = msg->adClient;
                newClient.port = msg->port;
                newClient.dSClient = dS;
                
                *clientList = addClient(*clientList, newClient);
                printf("✅ Nouvel utilisateur %s enregistré et connecté\n", msg->username);
            } else {
                strcpy(response.msg, "Erreur lors de l'enregistrement");
            }
        }
        
        free(content);
        close_file(fm);
    } else {
        strcpy(response.msg, "Erreur serveur - fichier utilisateurs");
    }

    sendMessageToClient(&response, dS, &msg->adClient);
}

static void cmd_disconnect(struct msgBuffer* msg, int dS, User* users, int nbUsers, Command* cmd, ClientNode** clientList) {
    struct msgBuffer response;
    memset(&response, 0, sizeof(response));
    strcpy(response.username, "server");
    response.adClient = msg->adClient;
    response.port = msg->port;

    bool client_removed = removeClient(clientList, msg->username);

    if (client_removed) {
        strcpy(response.msg, "Déconnexion réussie");
        printf("❌ Client %s déconnecté\n", msg->username);
    } else {
        strcpy(response.msg, "Client non trouvé dans la liste des connectés");
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
    } else if (strncmp(input, "@disconnect", 11) == 0) {
        cmd->type = CMD_DISCONNECT;
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
        case CMD_DISCONNECT:
            cmd_disconnect(msg, dS, users, nbUsers, cmd, clientList);
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