#include "command.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "message.h"
#include "client_list.h"
#include "file_manager.h"
#include "salon.h"
#include <signal.h>
#include <pthread.h>

static void cmd_help(struct msgBuffer* msg, int dS) {
    FileManager* fm = open_file("help.txt", "r"); 
    //FILE* f = fopen("help.txt", "r");
    struct msgBuffer response;
    memset(&response, 0, sizeof(response));
    strcpy(response.username, "server");
    response.adClient = msg->adClient;
    response.port = msg->port;
    response.opCode = 3;
    if (fm) {
        strcpy(response.msg, read_all(fm));
        //fread(response.msg, 1, MAX_MSG_LEN-1, f);
        close_file(fm);
        //fclose(f);
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
    response.opCode = 3;
    sendMessageToClient(&response, dS, &msg->adClient);
}
static void cmd_connect(struct msgBuffer* msg, int dS, Command* cmd, ClientNode** clientList, struct client c) {
    FileManager* fm = open_file("users.txt", "a+"); // a+ pour lire ET écrire, créer si n'existe pas
    struct msgBuffer response;
    memset(&response, 0, sizeof(response));
    strcpy(response.username, "server");
    response.adClient = msg->adClient;
    response.port = msg->port;
    response.opCode = 3;
    
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
            int dummy_admin_flag;
            if (sscanf(line, "%49s %49s %d", username, password, &dummy_admin_flag) >= 2) {
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
                
                struct client newClient;
                strcpy(newClient.username, msg->username);
                newClient.adClient = msg->adClient;
                newClient.port = msg->port;
                newClient.dSClient = dS;
                
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
                snprintf(new_user_line, sizeof(new_user_line), "%s %s %s\n", msg->username, msg->password, "0");
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
//commande non demandée, aucune utilisation concrète
static void cmd_disconnect(struct msgBuffer* msg, int dS, Command* cmd, ClientNode** clientList) {
    struct msgBuffer response;
    memset(&response, 0, sizeof(response));
    strcpy(response.username, "server");
    response.adClient = msg->adClient;
    response.port = msg->port;
    response.opCode = 3;

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
    FileManager* fm = open_file("credits.txt", "r");
    struct msgBuffer response;
    memset(&response, 0, sizeof(response));
    strcpy(response.username, "server");
    response.adClient = msg->adClient;
    response.port = msg->port;
    response.opCode = 3;
    if (fm) {
        strcpy(response.msg, read_all(fm));
        close_file(fm);
    } else {
        strcpy(response.msg, "Crédits indisponibles.");
    }
    sendMessageToClient(&response, dS, &msg->adClient);
}

static void cmd_shutdown(struct msgBuffer* msg, int dS) {
    struct msgBuffer response;
    memset(&response, 0, sizeof(response));
    strcpy(response.username, "server");
    response.adClient = msg->adClient;
    response.port = msg->port;
    response.opCode = 3;
    
    strcpy(response.msg, "🔴 Arrêt du serveur en cours...");
    sendMessageToClient(&response, dS, &msg->adClient);
    
    printf("🔴 Commande shutdown reçue de %s\n", msg->username);
    
    // Envoyer le signal SIGTERM au processus pour déclencher l'arrêt
    raise(SIGTERM);
}

static void cmd_msg(struct msgBuffer* msg, int dS, ClientNode* clientList, Command* cmd) {
    struct msgBuffer response;
    memset(&response, 0, sizeof(response));
    strcpy(response.username, "server");
    response.adClient = msg->adClient;
    response.port = msg->port;
    response.opCode = 3;
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
    response.opCode = 3;
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
static void cmd_who(struct msgBuffer* msg, int dS, struct client c) {
    struct msgBuffer response;
    response.opCode = 3;
    strcpy(response.username, "Serveur");

    char* nomSalon = trouverSalonDuClient(c);
    if (nomSalon == NULL) {
        strcpy(response.msg, "❌ Tu n'es dans aucun salon.");
    } else {
        int idx = salonExiste(nomSalon);
        if (idx == -1) {
            strcpy(response.msg, "❌ Salon introuvable.");
        } else {
            Salon s = salons[idx];
            char info[512] = "";
            snprintf(info, sizeof(info), "👥 Membres du salon \"%s\" :\n", s.nom);
            ClientNode* current = salons[idx].clients;
            while (current != NULL) {
                strcat(info, " - ");
                strcat(info, current->data.username);
                strcat(info, "\n");
                current = current->next;
            }

            strcpy(response.msg, info);
        }
    }
    sendMessageToClient(&response, dS, &msg->adClient);
}

static void cmd_join(int dS, char* arg1, struct msgBuffer* msg, struct client c) {
    char nomSalon[64];
    strcpy(nomSalon, arg1);

    struct msgBuffer response;
    response.opCode = 3;
    response.adClient = c.adClient;
    strcpy(response.username, "Serveur");

    // 🔎 Vérifier si le client est déjà dans un salon
    char* salonActuel = trouverSalonDuClient(c);
    if (salonActuel != NULL && strcmp(salonActuel, nomSalon) != 0) {
        // ✅ Le client est dans un autre salon → on le retire
        retirerClientDuSalon(salonActuel, c);

        // 📢 Notifier les autres
        struct msgBuffer notifLeave;
        notifLeave.opCode = 3;
        notifLeave.adClient = c.adClient;
        strcpy(notifLeave.username, "Serveur");
        snprintf(notifLeave.msg, sizeof(notifLeave.msg), "📢 %s a quitté le salon \"%s\".", c.username, salonActuel);
        int idxLeave = salonExiste(salonActuel);
        if (idxLeave != -1) {
            envoyerMessageAListe(salons[idxLeave].clients, &notifLeave, dS, &c);

        }
    }

    // 🔁 Continuer avec la logique join
    int idx = salonExiste(nomSalon);
    if (idx == -1) {
        strcpy(response.msg, "❌ Salon introuvable.");
        sendMessageToClient(&response, dS, &msg->adClient);
    } else {
        int result = ajouterClientAuSalon(nomSalon, c);
        if (result == 0) {
            sprintf(response.msg, "✅ Tu as rejoint le salon \"%s\".", nomSalon);
            sendMessageToClient(&response, dS, &msg->adClient);

            // ✅ Notifier les autres
            struct msgBuffer notif;
            notif.opCode = 3;
            notif.adClient = c.adClient;
            strcpy(notif.username, "Serveur");
            snprintf(notif.msg, sizeof(notif.msg), "📢 %s a rejoint le salon \"%s\".", c.username, nomSalon);
            envoyerMessageAListe(salons[idx].clients, &notif, dS, &c);
        } else if (result == -2) {
            sprintf(response.msg, "⚠️ Tu es déjà dans le salon \"%s\".", nomSalon);
            sendMessageToClient(&response, dS, &msg->adClient);
        } else {
            sprintf(response.msg, "❌ Impossible de rejoindre \"%s\" (plein ?).", nomSalon);
            sendMessageToClient(&response, dS, &msg->adClient);
        }
    }
}
static void cmd_info(int dS, struct client c, struct msgBuffer* msg) {
    char *nomSalon = msg->msg + 6;
    int idx = salonExiste(nomSalon);
    struct msgBuffer response;
    response.opCode = 3;
    response.adClient = c.adClient;
    strcpy(response.username, "Serveur");
    if (idx == -1) {
        strcpy(response.msg, "❌ Salon introuvable.");
    } else {
        Salon s = salons[idx];
        char info[512] = "";
        sprintf(info, "📂 Salon \"%s\" (%d membres):\n", s.nom, countClients(s.clients));
        ClientNode* current = salons[idx].clients;
        while (current != NULL) {
            strcat(info, " - ");
            strcat(info, current->data.username);
            strcat(info, "\n");
            current = current->next;
        }

        strcpy(response.msg, info);
    }

    sendMessageToClient(&response, dS, &msg->adClient);
}

static void cmd_leave(int dS, struct client c, struct msgBuffer* msg) {
    char* nomSalon = trouverSalonDuClient(c);

    struct msgBuffer response;
    response.opCode = 3;
    response.adClient = c.adClient;
    strcpy(response.username, "Serveur");

    if (nomSalon == NULL) {
        strcpy(response.msg, "❌ Tu n'es dans aucun salon.");
        sendMessageToClient(&response, dS, &msg->adClient);
    } else {
        char salonName[64];
        strcpy(salonName, nomSalon);
        int idx = salonExiste(nomSalon);


        struct msgBuffer notif;
        notif.opCode = 3;
        notif.adClient = c.adClient;
        strcpy(notif.username, "Serveur");
        snprintf(notif.msg, sizeof(notif.msg), "📢 %s a quitté le salon \"%s\".", c.username, salonName);


        if (idx != -1) {
            ClientNode* current = salons[idx].clients;
            while (current != NULL) {
                if (strcmp(current->data.username, c.username) != 0) { // Exclure le client qui part
                    sendMessageToClient(&notif, dS, &current->data.adClient);
                }
                current = current->next;
            }
        }


        retirerClientDuSalon(nomSalon, c);

        // ✅ Message au client
        sprintf(response.msg, "✅ Tu as quitté le salon \"%s\".", nomSalon);
        sendMessageToClient(&response, dS, &msg->adClient);
        
    }
}

static void cmd_create(int dS, char* arg1, struct client c, struct msgBuffer* msg) {
    char nomSalon[64];
    strcpy(nomSalon, arg1);
        
    struct msgBuffer response;
    response.opCode = 3;
    response.adClient = c.adClient;
    strcpy(response.username, "Serveur");

    if (creerSalon(nomSalon) == 0) {
        
        // ✅ Retirer l’utilisateur d’un éventuel salon existant
        char* salonActuel = trouverSalonDuClient(c);

        printf("🔍 Salon actuel du client %s : %s\n", c.username, salonActuel ? salonActuel : "Aucun");

        if (salonActuel != NULL) {
            retirerClientDuSalon(salonActuel, c);
            printf("✅ Client retiré de %s\n", salonActuel);
            

            // Notif aux autres
            int idxOld = salonExiste(salonActuel);
            if (idxOld != -1) {
                struct msgBuffer notifLeave;
                notifLeave.opCode = 3;
                notifLeave.adClient = c.adClient;
                strcpy(notifLeave.username, "Serveur");
                snprintf(notifLeave.msg, sizeof(notifLeave.msg), "📢 %s a quitté le salon \"%s\".", c.username, salonActuel);
                envoyerMessageAListe(salons[idxOld].clients, &notifLeave, dS, &c);
            }
        }

        // ✅ Ajouter au nouveau salon
        ajouterClientAuSalon(nomSalon, c);
        afficherinfoSalon(nomSalon);

        // ✅ Envoyer message de succès
        strcpy(response.msg, "✅ Salon créé et rejoint.");
        sendMessageToClient(&response, dS, &msg->adClient);
    } else {
        strcpy(response.msg, "❌ Erreur : salon déjà existant ou limite atteinte.");
        sendMessageToClient(&response, dS, &msg->adClient);
    }
}

static void cmd_channels(int dS, struct client c) {
    struct msgBuffer rsp;
    rsp.opCode   = 3;             // réponse serveur
    rsp.adClient = c.adClient;    // destinataire
    strcpy(rsp.username, "Serveur");

    // Si pas de salon, on prévient
    if (nb_salons == 0) {
        strcpy(rsp.msg, "Aucun salon disponible.");
        sendMessageToOneClient(c, &rsp, dS);
    }

    // Sinon on envoie un salon par message
    for (int i = 0; i < nb_salons; i++) {
        snprintf(rsp.msg, sizeof(rsp.msg),
                "Salon %d: %s", i+1, salons[i].nom);
        sendMessageToOneClient(c, &rsp, dS);
    }
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
    } else if (strncmp(input, "@who", 4) == 0) {
        cmd->type = CMD_WHO;
    } else if (strncmp(input, "@join", 5) == 0) {
        cmd->type = CMD_JOIN;
        const char* p = input + 5;
        while (*p == ' ') p++;
        sscanf(p, "%63s", cmd->arg1);
    }
    else if (strncmp(input, "@create", 7) == 0) {
        cmd->type = CMD_CREATE;
        const char* p = input + 7;
        while (*p == ' ') p++;
        sscanf(p, "%63s", cmd->arg1);
    }
    else if (strncmp(input, "@info", 5) == 0) {
        cmd->type = CMD_INFO;
    }
    else if (strncmp(input, "@leave", 6) == 0) {
        cmd->type = CMD_LEAVE;
    }
    else if (strncmp(input, "@channels", 9) == 0) {
        cmd->type = CMD_CHANNELS;
    }
    else {
        cmd->type = CMD_UNKNOWN;
    }
}

// Fonction centrale
void traiterCommande(Command* cmd, struct msgBuffer* msg, int dS, ClientNode** clientList, struct client c) {
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
            cmd_shutdown(msg, dS);
            break;
        case CMD_MSG:
            cmd_msg(msg, dS, *clientList, cmd);
            break;
        case CMD_LIST:
            cmd_list(msg, dS, *clientList);
            break;
        case CMD_CONNECT:
            cmd_connect(msg, dS, cmd, clientList, c);
            break;
        case CMD_DISCONNECT:
            cmd_disconnect(msg, dS, cmd, clientList);
            break;
        case CMD_CREATE:
            cmd_create(dS, cmd->arg1, c, msg);
            break;
        case CMD_WHO:
            cmd_who(msg, dS, c);
            break;
        case CMD_JOIN:
            cmd_join(dS, cmd->arg1, msg, c);
            break;
        case CMD_INFO:
            cmd_info(dS, c, msg);
            break;
        case CMD_LEAVE:
            cmd_leave(dS, c, msg);
            break;    
        case CMD_CHANNELS:
            cmd_channels(dS, c);
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