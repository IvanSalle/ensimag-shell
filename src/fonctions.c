#include "fonctions.h"

struct job* liste_job = NULL;


void add_jobs(int pid,char* nom){
	//ajoute en tête
	job* new_job = malloc(sizeof(job));
	new_job->pid = pid;
	new_job->nom = strdup(nom);
    // ajoute le temps de début
    struct timeval time;
    int ret = gettimeofday(&time,NULL);
    new_job->start_time = time;
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

char **supprimer_element_liste(char **liste, int index) {

    int n = 0;
    while (liste[n]) n++;

    if (index < 0 || index >= n) return liste;

    free(liste[index]);

    for (int i = index; i < n; i++) { // decaler a gauche
        liste[i] = liste[i + 1]; 
    }
    return liste;
}

char* remplacer_joker(struct cmdline* l,int cmd){
    // message d'erreur vaut null si pas d'erreur
    char* message = NULL;

    int count_param = 0;
    while(l->seq[cmd][count_param] != NULL){ // on compte les parametres 
        count_param++;
    }

    char** liste_finale = malloc(sizeof(char*));
    liste_finale[0] = NULL;


    for(int i = 0; i<count_param;i++){ // pour chacun des param
        char* mot = strdup(l->seq[cmd][i]);

        char** mots_a_traite = malloc(2*sizeof(char*) );// alloue le mot + le NULL
        mots_a_traite[0] = mot;
        mots_a_traite[1] = NULL; 
        int mot_courant = 0;

        while(mots_a_traite[mot_courant] != NULL){ // tant qu'il reste des mots a traiter
            mot = strdup(mots_a_traite[mot_courant]);
            
            char* accolade = strchr(mot, '{');
            char* etoile = strchr(mot, '*');
            char* tilde = strchr(mot, '~');
            char* dollar = strchr(mot, '$');
            char* interogation = strchr(mot, '?');
            char* crochet = strchr(mot, '[');
            if (accolade) {
                mots_a_traite = ajouter_listes(mots_a_traite,gerer_accolade_mot_simple(mot));
                mots_a_traite = supprimer_element_liste(mots_a_traite,mot_courant);
                break;
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
                mots_a_traite = supprimer_element_liste(mots_a_traite,mot_courant);

            }
            else if (etoile || interogation || crochet) {
                glob_t g;
                int ret = glob(mot, 0, NULL, &g);
                switch (ret) {
                    case 0:  // succès
                        mots_a_traite = ajouter_listes(mots_a_traite,g.gl_pathv);
                        globfree(&g);
                        break;

                    case GLOB_NOMATCH:  // pas de correspondance
                        message = malloc(strlen(mot) + 50);
                        sprintf(message, "Aucun fichier ne correspond au motif '%s'", mot);
                        break;

                    case GLOB_ABORTED:
                        message = malloc(strlen(mot) + 50);
                        sprintf(message, "Erreur d'accès au filesystem pour '%s'", mot);
                        break;

                    case GLOB_NOSPACE:
                        message = malloc(strlen(mot) + 50);
                        sprintf(message, "Mémoire insuffisante pour traiter '%s'", mot);
                        break;

                    default:
                        message = malloc(strlen(mot) + 50);
                        sprintf(message, "Erreur inconnue (%d) pour '%s'", ret, mot);
                        break;
                }
                mots_a_traite = supprimer_element_liste(mots_a_traite,mot_courant);
            }
            else{
                mot_courant++;
            }
            
            free(mot);
            
            
        }
        liste_finale = ajouter_listes(liste_finale,mots_a_traite);
        
        for (int j = 0; mots_a_traite[j]; j++) free(mots_a_traite[j]);

        free(mots_a_traite);   
    }
    for (int j = 0;j < count_param;j++) free(l->seq[cmd][j]);
    free(l->seq[cmd]);
    l->seq[cmd] = liste_finale;
    return message;
}

void handler_childsig(int sig) {
    (void)sig;

    int status;
    pid_t pid;

    // Récupérer tous les fils terminés
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        job* cur = liste_job;
        while (cur) {
            if (cur->pid == pid) {
                struct timeval cur_time;
                gettimeofday(&cur_time, NULL);

                long seconds  = cur_time.tv_sec  - cur->start_time.tv_sec;
                long useconds = cur_time.tv_usec - cur->start_time.tv_usec;
                if (useconds < 0) {
                    seconds -= 1;
                    useconds += 1000000;
                }

                long hours   = seconds / 3600;
                long minutes = (seconds % 3600) / 60;
                long secs    = seconds % 60;

                char buf[256];
                int len = snprintf(buf, sizeof(buf),
                                   "pid: %d nom: %s terminé après : %02ld:%02ld:%02ld:%06ld\n",cur->pid, cur->nom, hours, minutes, secs, useconds);
                write(STDOUT_FILENO, buf, len);
                break;
            }
            cur = cur->suivant;
        }
    }
}


void initialiser_sigchild(){
    struct sigaction sa;
    sa.sa_sigaction = handler_childsig;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD, &sa, NULL);
}

void exec_cmd_simple(struct cmdline* l){
		char* message_erreur = remplacer_joker(l,0);

         if (message_erreur) {
            printf("Erreur : %s\n", message_erreur);
            free(message_erreur);
            return;
        }

        
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

                char* message_erreur = remplacer_joker(l,i);

                if (message_erreur) {
                    printf("Erreur : %s\n", message_erreur);
                    free(message_erreur);
                    return;
                }
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
                else{
                    if(i>0){
                        close(cur_pipe[0]);
                        close(cur_pipe[1]); 
                    }
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