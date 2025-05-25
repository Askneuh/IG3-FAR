#include "salon.h"
#include "message.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define SAVE_FILE "save_salons.txt"

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
    if (nb_salons >= MAX_SALONS) {
        printf("❌ Impossible de créer \"%s\" : capacité max atteinte (%d).\n", nom, MAX_SALONS);
        return -1;
    }
    if (salonExiste(nom) != -1) {
        printf("❌ Le salon \"%s\" existe déjà.\n", nom);
        return -1;
    }

    strcpy(salons[nb_salons].nom, nom);
    salons[nb_salons].clients = NULL;
    nb_salons++;
    printf("✅ Salon \"%s\" créé (total : %d salon(s)).\n", nom, nb_salons);

    ;
    return 0;
}

int ajouterClientAuSalon(const char *nom, struct client c) {
    int idx = salonExiste(nom);
    if (idx == -1) {
        printf("❌ Impossible d’ajouter au salon \"%s\" : introuvable.\n", nom);
        return -1;
    }

    ClientNode* current = salons[idx].clients;
    while (current) {
        if (current->data.adClient.sin_addr.s_addr == c.adClient.sin_addr.s_addr &&
            current->data.adClient.sin_port == c.adClient.sin_port)
        {
            printf("⚠️  Client \"%s\" déjà dans \"%s\".\n", c.username, nom);
            return -2;
        }
        current = current->next;
    }

    salons[idx].clients = addClient(salons[idx].clients, c);
    printf("✅ Client \"%s\" ajouté au salon \"%s\".\n", c.username, nom);

    ;
    return 0;
}

int retirerClientDuSalon(const char *nom, struct client c) {
    int idx = salonExiste(nom);
    if (idx == -1) {
        printf("❌ Impossible de retirer du salon \"%s\" : introuvable.\n", nom);
        return -1;
    }

    ClientNode* prev = NULL;
    ClientNode* current = salons[idx].clients;
    while (current) {
        if (current->data.adClient.sin_addr.s_addr == c.adClient.sin_addr.s_addr &&
            current->data.adClient.sin_port == c.adClient.sin_port)
        {
            if (prev)
                prev->next = current->next;
            else
                salons[idx].clients = current->next;

            printf("✅ Client \"%s\" retiré du salon \"%s\".\n", c.username, nom);
            free(current);
            ;
            return 0;
        }
        prev = current;
        current = current->next;
    }

    printf("⚠️  Client \"%s\" non trouvé dans \"%s\".\n", c.username, nom);
    ;
    return -1;
}

void afficherSalons(void) {
    printf("📁 Liste des %d salon(s) :\n", nb_salons);
    for (int i = 0; i < nb_salons; i++) {
        printf("   - %s\n", salons[i].nom);
    }
}

void afficherinfoSalon(const char *nomSalon) {
    int idx = salonExiste(nomSalon);
    if (idx == -1) {
        printf("❌ Salon \"%s\" introuvable.\n", nomSalon);
        return;
    }

    Salon *s = &salons[idx];
    printf("\n📂 Infos du salon \"%s\" :\n", s->nom);

    int count = 0;
    for (ClientNode* cur = s->clients; cur; cur = cur->next)
        count++;

    printf("   • Membres : %d\n", count);
    if (count == 0) {
        printf("     (Aucun membre)\n");
    } else {
        for (ClientNode* cur = s->clients; cur; cur = cur->next) {
            printf("     - %s\n", cur->data.username);
        }
    }
    printf("-------------------------------\n");
}

char* trouverSalonDuClient(struct client c) {
    for (int i = 0; i < nb_salons; i++) {
        for (ClientNode* cur = salons[i].clients; cur != NULL; cur = cur->next) {
            if (strcmp(cur->data.username, c.username) == 0) {
                return salons[i].nom;
            }
        }
    }
    return NULL;
}

/**
 * Sauvegarde la liste des salons (leurs noms) dans SAVE_FILE.
 * Uniquement **une** définition ici.
 */
void sauvegarderSalons(void) {
    printf("📂 Sauvegarde des salons et de leurs membres dans \"%s\"…\n", SAVE_FILE);
    FILE *f = fopen(SAVE_FILE, "w");
    if (!f) {
        perror("⚠️  sauvegarderSalons fopen");
        return;
    }
    for (int i = 0; i < nb_salons; i++) {
        // Écrit le nom du salon
        fprintf(f, "%s;", salons[i].nom);

        // Parcourt la liste chaînée des clients
        ClientNode *cur = salons[i].clients;
        while (cur) {
            fprintf(f, "%s", cur->data.username);
            cur = cur->next;
            if (cur) fprintf(f, ",");
        }
        fprintf(f, "\n");
    }
    fclose(f);
    printf("✅ Sauvegarde terminée : %d salon(s) avec membres enregistré(s).\n", nb_salons);
}


void chargerSalons(void) {
    printf("📂 Chargement des salons et de leurs membres depuis \"%s\"…\n", SAVE_FILE);
    FILE *f = fopen(SAVE_FILE, "r");
    if (!f) {
        printf("⚠️  Aucun fichier \"%s\" trouvé, démarrage à vide.\n", SAVE_FILE);
        return;
    }

    char line[1024];
    int loaded = 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;

        // Découpe salon / membres
        char *nomSalon = strtok(line, ";");
        char *liste = strtok(NULL, ";");  // peut être NULL si pas de membres

        // Crée (ou ignore si existe déjà)
        if (creerSalon(nomSalon) == 0) {
            printf("   + Salon \"%s\" chargé.\n", nomSalon);
            loaded++;
        }

        // Parse la liste des pseudos
        if (liste) {
            char *pseudo = strtok(liste, ",");
            while (pseudo) {
                // On crée un client « factice » juste pour stocker le pseudo
                struct client fake = { .username = "" };
                strncpy(fake.username, pseudo, sizeof(fake.username)-1);
                fake.username[sizeof(fake.username)-1] = '\0';

                // On ajoute en mémoire
                ajouterClientAuSalon(nomSalon, fake);
                pseudo = strtok(NULL, ",");
            }
        }
    }
    fclose(f);
    printf("✅ Chargement terminé : %d salon(s) chargé(s) avec leurs membres.\n", loaded);
}
