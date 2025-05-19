#ifndef SALON_H
#define SALON_H

#define MAX_NAME 50
#define MAX_SALONS 20

#include "client_list.h"

typedef struct {
    char nom[MAX_NAME];
    ClientNode* clients;  // ✅ Liste chaînée de clients
} Salon;

extern Salon salons[MAX_SALONS];
extern int nb_salons;

int creerSalon(const char *nom);
int ajouterClientAuSalon(const char *nom, struct client c);
int retirerClientDuSalon(const char *nom, struct client c);
int salonExiste(const char *nom);
void afficherSalons();
void afficherinfoSalon(const char *nomSalon);
char* trouverSalonDuClient(struct client c);
void chargerSalons(void);
void sauvegarderSalons(void);
int pseudoDansSalon(const char *nom, const char *pseudo);
void restaurerClientDansSesSalons(struct client c);

#endif
