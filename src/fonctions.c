#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <wordexp.h>
#include <glob.h>
#include "variante.h"
#include "readcmd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Retourne un tableau de mots (char**), terminé par NULL
// Exemple : "a{b,c}d" → ["abd", "acd", NULL]
char **gerer_accolade_mot_simple(const char *word) {
    const char *open = strchr(word, '{');
    if (!open) {
        // Aucun joker d'accolade → tableau avec un seul mot
        char **res = malloc(2 * sizeof(char *));
        res[0] = strdup(word);
        res[1] = NULL;
        return res;
    }

    const char *close = strchr(open, '}');
    if (!close || close < open) {
        // Accolades mal formées
        fprintf(stderr, "Erreur: accolades non fermées dans '%s'\n", word);
        char **res = malloc(2 * sizeof(char *));
        res[0] = strdup(word);
        res[1] = NULL;
        return res;
    }

    // Extraire préfixe et suffixe
    size_t prefix_len = open - word;
    char prefix[256];
    strncpy(prefix, word, prefix_len);
    prefix[prefix_len] = '\0';

    const char *suffix = close + 1;

    // Extraire contenu entre accolades
    size_t brace_len = close - open - 1;
    char *brace = strndup(open + 1, brace_len);

    // Tokenisation par virgule
    char **res = malloc(64 * sizeof(char *)); // taille initiale, à agrandir si besoin
    int count = 0;

    char *token = strtok(brace, ",");
    while (token) {
        char tmp[512];
        snprintf(tmp, sizeof(tmp), "%s%s%s", prefix, token, suffix);
        res[count++] = strdup(tmp);
        token = strtok(NULL, ",");
    }

    res[count] = NULL; // fin du tableau
    free(brace);

    return res;
}

char **ajouter_listes(char **liste_dest, char **liste_src) {
    // Compter les tailles
    int n_dest = 0, n_src = 0;
    while (liste_dest && liste_dest[n_dest]) n_dest++;
    while (liste_src && liste_src[n_src]) n_src++;

    // Reallocation
    liste_dest = realloc(liste_dest, (n_dest + n_src + 1) * sizeof(char*));

    // ajouter 
    for (int i = 0; i < n_src; i++) {
        liste_dest[n_dest + i] = strdup(liste_src[i]);
    }
    // mettre le cahr null a la fin
    liste_dest[n_dest + n_src] = NULL;
    return liste_dest;
}


void remplacer_joker(struct cmdline* l,int cmd){

    int count_param = 0;
    while(l->seq[cmd][count_param] != NULL){ // on compte les parametres 
        count_param++;
    }

    char** liste_finale = malloc(sizeof(char*));
    liste_finale[0] = NULL;


    for(int i = 0; i<count_param;i++){ // pour chacun des param
        char* mot = strdup(l->seq[cmd][i]);
        //
        char** mots_a_traite = malloc(2*sizeof(char*) );// alloue le mot + le NULL
        mots_a_traite[0] = mot;
        mots_a_traite[1] = NULL; 
        int mot_courant = 0;

        while(mots_a_traite[mot_courant] != NULL){ // tant qu'il reste des mots a traiter
            mot = mots_a_traite[mot_courant];
            char* accolade = strchr(mot, '{');
            char* etoile = strchr(mot, '*');
            char* tilde = strchr(mot, '~');
            char* dollar = strchr(mot, '$');
            char* interogation = strchr(mot, '?');
            char* crochet = strchr(mot, '[');
            while(accolade || etoile || tilde || dollar || interogation || crochet){ //tant qu'il y a des jokers a traiter
                if (accolade) {
                    mots_a_traite = ajouter_listes(mots_a_traite,gerer_accolade_mot_simple(mot));
                }
                else if (tilde || dollar) {
                    wordexp_t w;
                    int ret = wordexp(mot,&w,0);
                    int taille = strlen(mot);
                    if (ret != 0){
                        fprintf(stderr, "wordexp failed: %d\n", ret);
                    } 
                    else{
                        mots_a_traite = ajouter_listes(mots_a_traite,w.we_wordv);
                        wordfree(&w);
                    }
                    
                }
                else if (etoile || interogation || crochet) {
                    glob_t g;
                    int ret = glob(mot, 0, NULL, &g);
                    if (ret != 0){
                        fprintf(stderr, "glob failed: %d\n", ret);
                    }
                    else{
                        mots_a_traite = ajouter_listes(mots_a_traite,g.gl_pathv);
                        globfree(&g);
                    }
                    
                }             
            }
            mot_courant++;
            
        }
        liste_finale = ajouter_listes(liste_finale,mots_a_traite);
        
        for (int j = 0; mots_a_traite[j]; j++) free(mots_a_traite[j]);

        free(mots_a_traite);

        for (int j = 0;j < count_param;j++) free(l->seq[cmd][j]);

        l->seq[cmd] = liste_finale;    
    }
}