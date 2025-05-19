#ifndef COMMAND_H
#define COMMAND_H

#define MAX_CMD_LEN 32
#define MAX_ARG_LEN 128

typedef enum {
    CMD_HELP,
    CMD_PING,
    CMD_MSG,
    CMD_CREDITS,
    CMD_SHUTDOWN,
    CMD_CONNECT,
    CMD_LIST,
    CMD_CREATE,
    CMD_UNKNOWN
} CommandType;

typedef struct {
    CommandType type;
    char arg1[MAX_ARG_LEN];
    char arg2[MAX_ARG_LEN];
} Command;

Command parseCommand(const char* msg);
const char* commandTypeToString(CommandType type);

#endif