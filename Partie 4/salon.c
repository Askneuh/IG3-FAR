#include "salon.h"
#include "message.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

Salon salons[MAX_SALONS];
int nb_salons = 0;

int salonExiste(const char *nom) {
    for (int i = 0; i < nb_salons; i++) {
        if (strcmp(salons[i].nom, nom) == 0)
            return i;
    }
    return -1;
}

int creerSalon(const char *nom) {
    if (nb_salons >= MAX_SALONS || salonExiste(nom) != -1) return -1;
    strcpy(salons[nb_salons].nom, nom);
    salons[nb_salons].clients = NULL;
    nb_salons++;
    return 0;
}

int ajouterClientAuSalon(const char *nom, struct client c) {
    int idx = salonExiste(nom);
    if (idx == -1) return -1;

    // Vérifie si le client est déjà dans le salon
    ClientNode* current = salons[idx].clients;
    while (current != NULL) {
        if (current->data.adClient.sin_addr.s_addr == c.adClient.sin_addr.s_addr &&
            current->data.adClient.sin_port == c.adClient.sin_port) {
            return -2; // déjà présent
        }
        current = current->next;
    }

    // Ajout du client
    salons[idx].clients = addClient(salons[idx].clients, c);
    return 0;
}

int retirerClientDuSalon(const char *nom, struct client c) {
    int idx = salonExiste(nom);
    if (idx == -1) return -1;

    ClientNode* prev = NULL;
    ClientNode* current = salons[idx].clients;

    while (current != NULL) {
        if (current->data.adClient.sin_addr.s_addr == c.adClient.sin_addr.s_addr &&
            current->data.adClient.sin_port == c.adClient.sin_port) {

            if (prev == NULL) {
                salons[idx].clients = current->next;
            } else {
                prev->next = current->next;
            }
            free(current);
            return 0;
        }
        prev = current;
        current = current->next;
    }

    return -1;
}

void afficherSalons() {
    printf("📁 Liste des salons :\n");
    for (int i = 0; i < nb_salons; i++) {
        printf(" - %s\n", salons[i].nom);
    }
}

void afficherinfoSalon(const char *nomSalon) {
    int idx = salonExiste(nomSalon);
    if (idx == -1) {
        printf("❌ Salon \"%s\" introuvable.\n", nomSalon);
        return;
    }

    Salon s = salons[idx];
    printf("\n📂 Infos du salon \"%s\"\n", s.nom);

    int count = 0;
    ClientNode* current = s.clients;
    while (current != NULL) {
        count++;
        current = current->next;
    }

    printf("👥 Nombre de clients : %d\n", count);

    if (count == 0) {
        printf("(Aucun membre)\n");
    } else {
        current = s.clients;
        while (current != NULL) {
            printf("  - %s\n", current->data.username);
            current = current->next;
        }
    }

    printf("-------------------------------\n");
}

char* trouverSalonDuClient(struct client c) {
    for (int i = 0; i < nb_salons; i++) {
        ClientNode* current = salons[i].clients;
        while (current != NULL) {
            if (current->data.adClient.sin_addr.s_addr == c.adClient.sin_addr.s_addr &&
                current->data.adClient.sin_port == c.adClient.sin_port) {
                return salons[i].nom;
            }
            current = current->next;
        }
    }
    return NULL;
}
