#ifndef FONCTIONS_H
#define FONCTIONS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <wordexp.h>
#include <glob.h>
#include "readcmd.h"

#define STDIN 0
#define STDOUT 1
#define STDERR 2
#define TAILLE_PARAM_MAX 255


typedef struct job {
	int pid;
	char* nom;
	struct job* suivant;
} job;


/* ajoute une tâche de fond a la liste*/
void add_jobs(int pid,char* nom);

/* supprime les tâches de fond terminée de la liste*/
void delete_job();

/* affiche les tâches de fond de la liste*/
void print_jobs();

/* remplace les accolades par le ou les mots correspondants*/
char **gerer_accolade_mot_simple(const char *word);

/* ajoute une liste de mot a une liste deja alloué et la réalloue à la bonne taille*/
char **ajouter_listes(char **liste_dest, char **liste_src);

/* supprime un élément d'une liste et décale les suivants à gauche */
char **supprimer_element_liste(char **liste, int index);

/* remplace tous les jokers d'une liste de parametre par les mots correspondants et renvoie un code d'erreur sinon*/
char* remplacer_joker(struct cmdline* l,int cmd);

/* exécute une commande seule*/
void exec_cmd_simple(struct cmdline* l);

/* exécute plusieurs commandes avec des pipes entre*/
void exec_cmd_avec_pipes(struct cmdline* l);

#endif 