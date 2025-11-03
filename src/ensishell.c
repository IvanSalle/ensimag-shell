/*****************************************************
 * Copyright Grégory Mounié 2008-2015                *
 *           Simon Nieuviarts 2002-2009              *
 * This code is distributed under the GLPv3 licence. *
 * Ce code est distribué sous la licence GPLv3+.     *
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <wordexp.h>
#include <glob.h>

#define STDIN 0
#define STDOUT 1
#define STDERR 2
#define TAILLE_PARAM_MAX 255

#include "variante.h"
#include "readcmd.h"
#ifndef VARIANTE
#error "Variante non défini !!"
#endif

/* Guile (1.8 and 2.0) is auto-detected by cmake */
/* To disable Scheme interpreter (Guile support), comment the
 * following lines.  You may also have to comment related pkg-config
 * lines in CMakeLists.txt.
 */

#if USE_GUILE == 1
#include <libguile.h>

int question6_executer(char *line)
{
	/* Question 6: Insert your code to execute the command line
	 * identically to the standard execution scheme:
	 * parsecmd, then fork+execvp, for a single command.
	 * pipe and i/o redirection are not required.
	 */

	
	struct cmdline* l = parsecmd( & line);
	exec_cmd_simple(l);
	return 0;
}

SCM executer_wrapper(SCM x)
{
        return scm_from_int(question6_executer(scm_to_locale_stringn(x, 0)));
}
#endif


void terminate(char *line) {
#if USE_GNU_READLINE == 1
	/* rl_clear_history() does not exist yet in centOS 6 */
	clear_history();
#endif
	if (line)
	  free(line);
	printf("exit\n");
	exit(0);
}

//fonction et variables ajoutées
typedef struct job {
	int pid;
	char* nom;
	struct job* suivant;
} job;

struct job* liste_job = NULL;
void add_jobs(int pid,char* nom){
	//ajoute en tête
	job* new_job = malloc(sizeof(job));
	new_job->pid = pid;
	new_job->nom = strdup(nom);
	if(liste_job == NULL){
		new_job->suivant = NULL;
	}
	else{
		new_job->suivant = liste_job;
	}
	liste_job = new_job;
}
void delete_job(){
	// teste tous les jobs pour supprimer ceux qui sont fini
	job* cur = liste_job;
	job* prev = NULL;
	int status;
	int ret;

	while(cur){
		ret = waitpid(cur->pid,&status,WNOHANG);
		if(ret == cur->pid){ // si le processus est arreté
			if(prev){
				prev->suivant = cur->suivant;
				free(cur->nom);
				free(cur);
				cur = prev->suivant;
			}
			else{
				liste_job = cur->suivant;
				free(cur->nom);
				free(cur);
				cur = liste_job;
			}
		}
		else{
			prev = cur;
            cur = cur->suivant;
		}
	}
}
void print_jobs(){
		job* cur = liste_job;
		printf("commandes en tâche de fond:\n");
		while(cur){
			printf(" pid: %i , commande: %s \n", cur->pid,cur->nom);
			cur = cur->suivant;
		}	
}
char* concat_et_aggrandi(char *dest, const char *src) { // concatene deux chaine et aggrandi si necessaire
    if (!dest) {
        dest = malloc(strlen(src) + 1);
        strcpy(dest, src);
    } else {
        size_t new_len = strlen(dest) + strlen(src) + 2; // "\0 et espace"
        dest = realloc(dest, new_len);
        strcat(dest, " ");
        strcat(dest, src);
    }
    return dest;
}
char* gerer_accolade_mot_simple(const char *word) {
    char *result = NULL;

    const char *open = strchr(word, '{');
    if (!open) return strdup(word); // le mot normal si pas d'accolade 

    char prefix[256];
    strncpy(prefix, word, open - word);
    prefix[open - word] = '\0';

    const char *close = strchr(open, '}');
    char brace[256];
    strncpy(brace, open + 1, close - open - 1);
    brace[close - open - 1] = '\0';

    const char *suffix = close + 1;

    char *token = strtok(brace, ",");
    while (token) {
        char tmp[512];
        snprintf(tmp, sizeof(tmp), "%s%s%s", prefix, token, suffix);
        if (!result) {
            result = strdup(tmp);
        } else {
            size_t len = strlen(result) + strlen(tmp) + 2;
            result = realloc(result, len);
            strcat(result, " ");
            strcat(result, tmp);
        }
        token = strtok(NULL, ",");
    }

    return result;
}
char* gerer_accolade_plusieurs_mots(const char *line) {
    char *result = NULL;
    char *copy = strdup(line); 
    char *mot = strtok(copy, " "); //

    while (mot) {
        char *mot_developpe = gerer_accolade_mot_simple(mot);
        if (!result) {
            result = strdup(mot_developpe);
        } else {
            size_t len = strlen(result) + strlen(mot_developpe) + 2;
            result = realloc(result, len);
            strcat(result, " ");
            strcat(result, mot_developpe);
        }
        free(mot_developpe);
        mot = strtok(NULL, " ");
    }
    free(copy);
    return result;
}

void remplacer_joker(struct cmdline* l,int cmd){
	// compter le nb de param pour cette commande
	int nb_param = 0;
	while(l->seq[cmd][nb_param] != NULL) {
				nb_param++;
	}
	// concatener les parametres en un char*
	int taille_str = 0;
	for (int j = 0; j < nb_param; j++) {
		taille_str += strlen(l->seq[cmd][j]) + 1;
	}
	taille_str++; // pour le \0
	char* str_param = calloc(taille_str, sizeof(char));
	for (int j = 0; j < nb_param; j++) {
			strlcat(str_param, l->seq[cmd][j],taille_str);
			if (j < nb_param - 1){
				strlcat(str_param, " ",taille_str);}
			
	}
	// on gere les accolades
	str_param = gerer_accolade_plusieurs_mots(str_param);
	wordexp_t w;
	int ret = wordexp(str_param,&w,0);
	if (ret != 0) fprintf(stderr, "wordexp failed: %d\n", ret);
	char** new_seq = NULL;
	int taille_new_seq = 0;
	for(int i = 0; i < w.we_wordc; i++){
		glob_t g;
		int ret = glob(w.we_wordv[i], 0, NULL, &g);
		if (ret != 0) fprintf(stderr, "glob failed: %d\n", ret);
		if(ret == 0){ // si correspondance
			for (int j = 0; j < g.gl_pathc; j++) { //on ajoute toutes les corresp. a la liste
                new_seq = realloc(new_seq, sizeof(char*) *(taille_new_seq + 1)); // on agrandit new_seq
                new_seq[taille_new_seq] = strdup(g.gl_pathv[j]);  // on agrandit new_seq
                taille_new_seq++;
            }
		}
		else{
			new_seq = realloc(new_seq, sizeof(char*) *(taille_new_seq + 1));
            new_seq[taille_new_seq] = strdup(w.we_wordv[i]); 
            taille_new_seq++;
		}
		globfree(&g);
	}
	// on ajoute le char de fin
	new_seq = realloc(new_seq, sizeof(char*) *(taille_new_seq + 1));
    	new_seq[taille_new_seq] = NULL;

		for (int j = 0; j < nb_param; j++) free(l->seq[cmd][j]);
		free(l->seq[cmd]);
		l->seq[cmd] = new_seq;
	wordfree(&w);
    free(str_param);
}
void exec_cmd_avec_pipes(struct cmdline* l){
			int nb_cmd = 0;
			while(l->seq[nb_cmd] != NULL) {
				nb_cmd++;
			}
			// variables
			int prev_pipe[2];
			int cur_pipe[2];
			int pid;
			int prev_existe = 0;
			int background = 0;
			int liste_pid[nb_cmd];
			for (int i=0; i<nb_cmd; i++){
				if(i < nb_cmd - 1) pipe(cur_pipe); // si pas dernier on creer un nouveau pipe
				pid = fork();
				if (pid == 0){
					if (l->in){ // si il y a un fichier en entrée
						int fd_in = open(l->in,O_RDONLY);
						dup2(fd_in,STDIN);
						close(fd_in);
					}
					if (l->out){ //si il y a un fichier en sortie
						int fd_out = open(l->out,O_WRONLY | O_CREAT, 0644); 
						ftruncate(fd_out, 0);
						dup2(fd_out,STDOUT);
						close(fd_out);
					}
					if(prev_existe){// si pas la premiere commande (si il y a un prev_pipe)
						// on met la lecture du pipe dans l'entrée standard
						dup2(prev_pipe[0],STDIN);
						close(prev_pipe[0]);
						close(prev_pipe[1]);
					}
					//si pas la derniere commande
					if(i < nb_cmd - 1  ){
						// on ecrit dans le pipe avec la sortie standard
						dup2(cur_pipe[1],STDOUT);
						close(cur_pipe[0]);
						close(cur_pipe[1]);
					}
					
					execvp(l->seq[i][0],l->seq[i]);
				}	
				if(prev_existe){
					close(prev_pipe[0]);
					close(prev_pipe[1]);
				}
				if(i < nb_cmd - 1){ // pas la derniere cmd
					prev_pipe[0] = cur_pipe[0];
					prev_pipe[1] = cur_pipe[1];
					prev_existe = 1;
				}
				if(l->bg) background = 1 ;// toutes les commandes s'execute en fond
				// on ajoute le pid a la liste
				liste_pid[i] = pid;
			}
			if(!background){
				int status;
				for(int i =0; i<nb_cmd; i++){
					waitpid(liste_pid[i],&status,0);
				}
			}
		}
void exec_cmd_simple(struct cmdline* l){
		remplacer_joker(l,0);
		// commande job 
			if (strcmp(l->seq[0][0], "jobs") == 0) {
				//int pid = fork();
				delete_job();
				print_jobs();
			}
			else{
				int pid = fork();
				if(pid == 0){
					if (l->in){ // si il y a un fichier en entrée
						int fd_in = open(l->in,O_RDONLY);
						dup2(fd_in,STDIN);
						close(fd_in);
					}
					if (l->out){ //si il y a un fichier en sortie
						int fd_out = open(l->out,O_WRONLY | O_CREAT, 0644); 
						ftruncate(fd_out, 0);
						dup2(fd_out,STDOUT);
						close(fd_out);
					}
					execvp(l->seq[0][0],l->seq[0]);
				}
				if(l->bg){ // tache de fond
					// on ajoute dans la liste
					add_jobs(pid,l->seq[0][0]);
				}
				else{
					int wstatus;
					int child_pid = wait(&wstatus);
				}
			}	
};

	
int main(){
        printf("Variante %d: %s\n", VARIANTE, VARIANTE_STRING);

#if USE_GUILE == 1
        scm_init_guile();
        /* register "executer" function in scheme */
        scm_c_define_gsubr("executer", 1, 0, 0, executer_wrapper);
#endif

	while (1) {
		struct cmdline *l;
		char *line=0;
		char *prompt = "ensishell>";

		/* Readline use some internal memory structure that
		   can not be cleaned at the end of the program. Thus
		   one memory leak per command seems unavoidable yet */
		line = readline(prompt);
		if (line == 0 || ! strncmp(line,"exit", 4)) {
			terminate(line);
		}

#if USE_GNU_READLINE == 1
		add_history(line);
#endif


#if USE_GUILE == 1
		/* The line is a scheme command */
		if (line[0] == '(') {
			char catchligne[strlen(line) + 256];
			sprintf(catchligne, "(catch #t (lambda () %s) (lambda (key . parameters) (display \"mauvaise expression/bug en scheme\n\")))", line);
			scm_eval_string(scm_from_locale_string(catchligne));
			free(line);
                        continue;
                }
#endif

		/* parsecmd free line and set it up to 0 */
		l = parsecmd( & line);

		/* If input stream closed, normal termination */
		if (!l) {
		  
			terminate(0);
		}
		
		if (l->err) {
			/* Syntax error, read another command */
			printf("error: %s\n", l->err);
			continue;
		}
		if(l->seq[1] != NULL){ // il a plus d'une commande (donc pipe)
			exec_cmd_avec_pipes(l);
		}
		else if(l->seq[0] != NULL){ // il y a une seule commande 
			exec_cmd_simple(l);
		} 
		
		/*
		if (l->in){
			printf("in: %s\n", l->in);
		} */
		//if (l->out) printf("out: %s\n", l->out);
		//if (l->bg) printf("background (&)\n");

		/* Display each command of the pipe */
		/*for (i=0; l->seq[i]!=0; i++) {
			char **cmd = l->seq[i];
			printf("seq[%d]: ", i);
                        for (j=0; cmd[j]!=0; j++) {
                                printf("'%s' ", cmd[j]);
                        }
			printf("\n");
		}*/
	}

}
