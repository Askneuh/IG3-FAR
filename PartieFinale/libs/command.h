#ifndef COMMAND_H
#define COMMAND_H

#include "message.h"
#include "client_list.h"
#include "file_manager.h"

typedef enum {
    CMD_HELP,
    CMD_PING,
    CMD_MSG,
    CMD_CREDITS,
    CMD_SHUTDOWN,
    CMD_LIST,
    CMD_CONNECT,
    CMD_UNKNOWN,
    CMD_DISCONNECT,
    CMD_WHO,
    CMD_JOIN,
    CMD_INFO,
    CMD_LEAVE,
    CMD_CREATE,
    CMD_CHANNELS
} CommandType;

typedef struct {
    CommandType type;
    char arg1[64]; 
    char arg2[256]; 
} Command;

void traiterCommande(Command* cmd, struct msgBuffer* msg, int dS, ClientNode** clientList, struct client c);

void parseCommand(const char* input, Command* cmd);

#endif