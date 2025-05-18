#include "command.h"
#include <string.h>
#include <stdio.h>

Command parseCommand(const char* msg) {
    Command cmd;
    memset(&cmd, 0, sizeof(Command));
    if (strncmp(msg, "@help", 5) == 0) {
        cmd.type = CMD_HELP;
    } else if (strncmp(msg, "@ping", 5) == 0) {
        cmd.type = CMD_PING;
    } else if (strncmp(msg, "@msg ", 5) == 0) {
        cmd.type = CMD_MSG;
        sscanf(msg + 5, "%s %[^\n]", cmd.arg1, cmd.arg2);
    } else if (strncmp(msg, "@credits", 8) == 0) {
        cmd.type = CMD_CREDITS;
    } else if (strncmp(msg, "@shutdown", 9) == 0) {
        cmd.type = CMD_SHUTDOWN;
    } else if (strncmp(msg, "@connect ", 9) == 0) {
        cmd.type = CMD_CONNECT;
        sscanf(msg + 9, "%s %s", cmd.arg1, cmd.arg2);
    } else if (strncmp(msg, "@list", 5) == 0) {
        cmd.type = CMD_LIST;
    } else {
        cmd.type = CMD_UNKNOWN;
    }
    return cmd;
}

const char* commandTypeToString(CommandType type) {
    switch (type) {
        case CMD_HELP: return "HELP";
        case CMD_PING: return "PING";
        case CMD_MSG: return "MSG";
        case CMD_CREDITS: return "CREDITS";
        case CMD_SHUTDOWN: return "SHUTDOWN";
        case CMD_CONNECT: return "CONNECT";
        case CMD_LIST: return "LIST";   
        default: return "UNKNOWN";
    }
}