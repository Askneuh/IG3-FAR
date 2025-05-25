#ifndef COMMANDES_H
#define COMMANDES_H

#include "message.h"
#include "clientList.h"
#include <stdbool.h>
#include <netinet/in.h>

// Traite une commande utilisateur et agit en conséquence
ClientNode* traiterCommande(struct msgBuffer* msg, ClientNode* list, int dS);

// Envoie un message formaté à un seul client
void sendToClient(const char* sender, const char* message, struct sockaddr_in addr, int dS);

// Diffuse un message à tous les clients sauf l'expéditeur
void sendAll(ClientNode* list, const char* message, struct sockaddr_in senderAddr, int dS);

// Diffuse un message à tous les clients (y compris l’expéditeur)
void sendAllToEveryone(ClientNode* list, const char* message, int dS);

// Envoie la liste des clients actuellement connectés
void sendList(ClientNode* list, struct sockaddr_in clientAddr, int dS);

// Lit et envoie le fichier d'aide
void sendHelp(struct sockaddr_in clientAddr, int dS);

// Lit et envoie le fichier des crédits
void sendCredits(struct sockaddr_in clientAddr, int dS);

// Gère l’envoi d’un message privé à un autre client
void sendPrivateMessage(ClientNode* list, struct msgBuffer* msg, int dS);

// Change le pseudo d’un utilisateur
void renameUser(ClientNode* list, struct msgBuffer* msg, int dS);

// Vérifie si les identifiants (login + mot de passe) sont valides
bool verifyCredentials(const char* username, const char* password);

#endif // COMMANDES_H
