#ifndef COMMAND_H
#define COMMAND_H

#include "message.h"
#include "client_list.h"
#include "users.h"
#include "file_manager.h"

// Enum pour les types de commandes
typedef enum {
    CMD_HELP,
    CMD_PING,
    CMD_MSG,
    CMD_CREDITS,
    CMD_SHUTDOWN,
    CMD_LIST,
    CMD_CONNECT,
    CMD_UNKNOWN,
    CMD_DISCONNECT
} CommandType;

// Structure pour une commande
typedef struct {
    CommandType type;
    char arg1[64]; // Premier argument (ex: nom utilisateur)
    char arg2[256]; // Deuxième argument (ex: message)
} Command;

// Prototype de la fonction centrale
void traiterCommande(Command* cmd, struct msgBuffer* msg, int dS, ClientNode** clientList, User* users, int nbUsers);

// Prototype pour parser une commande 
void parseCommand(const char* input, Command* cmd);

#endif