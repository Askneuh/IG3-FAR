#include "salon.h"
#include "message.h"
#include <string.h>
#include <stdio.h>

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
    salons[nb_salons].nb_clients = 0;
    nb_salons++;
    return 0;
}

int ajouterClientAuSalon(const char *nom, struct client c) {
    int idx = salonExiste(nom);
    if (idx == -1 || salons[idx].nb_clients >= MAX_CLIENTS) return -1;

    // Vérifier si le client est déjà dedans (pour éviter doublons)
    for (int i = 0; i < salons[idx].nb_clients; i++) {
        if (salons[idx].clients[i].adClient.sin_addr.s_addr == c.adClient.sin_addr.s_addr &&
            salons[idx].clients[i].adClient.sin_port == c.adClient.sin_port) {
            return -2; // déjà présent
        }
    }

    salons[idx].clients[salons[idx].nb_clients++] = c;
    return 0;
}

int retirerClientDuSalon(const char *nom, struct client c) {
    int idx = salonExiste(nom);
    if (idx == -1) return -1;

    for (int i = 0; i < salons[idx].nb_clients; i++) {
        if (salons[idx].clients[i].adClient.sin_addr.s_addr == c.adClient.sin_addr.s_addr &&
            salons[idx].clients[i].adClient.sin_port == c.adClient.sin_port) {
            
            for (int j = i; j < salons[idx].nb_clients - 1; j++) {
                salons[idx].clients[j] = salons[idx].clients[j + 1];
            }
            salons[idx].nb_clients--;
            return 0;
        }
    }

    return -1; // pas trouvé
}

void afficherSalons() {
    printf("📁 Liste des salons :\n");
    for (int i = 0; i < nb_salons; i++) {
        printf(" - %s (%d membres)\n", salons[i].nom, salons[i].nb_clients);
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
    printf("👥 Nombre de clients : %d\n", s.nb_clients);

    if (s.nb_clients == 0) {
        printf("(Aucun membre)\n");
    } else {
        for (int i = 0; i < s.nb_clients; i++) {
            printf("  - %s\n", s.clients[i].username);
        }
    }

    printf("-------------------------------\n");
}

