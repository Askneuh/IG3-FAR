#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "common.h"
#include "client_list.h"
#include "users.h"
#include "command.h"
#include "message.h"

#ifndef SERVER_COM_H
#define SERVER_COM_H

ClientNode* ReceiveMessage(int dS, struct msgBuffer* msg, ClientNode* clientList, struct sockaddr_in adServeur);
int sendMessageToAllClients(ClientNode* clientList, struct msgBuffer* msg, int serverSocket);
#endif // SERVER_COM_H