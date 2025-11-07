#include "fonctions.h"

job* liste_job = NULL;
int limite = -1;

// trouve le plus petit numéro de job disponible
int get_next_job_number() {
	if (!liste_job) return 1;
	
	int max_num = 0;
	job* cur = liste_job;
	while (cur) {
		if (cur->job_number > max_num) max_num = cur->job_number;
		cur = cur->suivant;
	}
	
	for (int i = 1; i <= max_num + 1; i++) {
		int used = 0;
		cur = liste_job;
		while (cur) {
			if (cur->job_number == i) {
				used = 1;
				break;
			}
			cur = cur->suivant;
		}
		if (!used) return i;
	}
	return 1;
}

void add_jobs(int pid, char* nom) {
	//ajoute en tête
	job* new_job = malloc(sizeof(job));
	new_job->job_number = get_next_job_number();
	new_job->pid = pid;
	new_job->nom = strdup(nom);
    // ajoute le temps de début
    struct timeval time;
    gettimeofday(&time, NULL);
    new_job->start_time = time;
	new_job->suivant = liste_job;
	liste_job = new_job;
	
	// Affichage comme dans un terminal : [n] pid
	printf("[%d] %d\n", new_job->job_number, new_job->pid);
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
			printf("[%d]  pid: %i , commande: %s \n", cur->job_number, cur->pid, cur->nom);
			cur = cur->suivant;
		}	
}

char **gerer_accolade_mot_simple(const char *word) {
    const char *search_start = word;
    const char *open = NULL;
    const char *close = NULL;
    
    // chercher la première paire d'accolades non vides
    while ((open = strchr(search_start, '{'))) {
        close = strchr(open, '}');
        
        // Accolade ouvrante sans fermante
        if (!close || close < open) {
            char **res = malloc(2 * sizeof(char *));
            res[0] = strdup(word);
            res[1] = NULL;
            return res;
        }
        
        size_t brace_len = close - open - 1;
        
        // Si accolades non vides, on les traite
        if (brace_len > 0) {
            break;
        }
        
        // accolades vides, continuer la recherche après
        search_start = close + 1;
    }
    
    // aucune accolade ou que des accolades vides
    if (!open || !close) {
        char **res = malloc(2 * sizeof(char *));
        res[0] = strdup(word);
        res[1] = NULL;
        return res;
    }

    // extraire préfixe et suffixe
    size_t prefix_len = open - word;
    char prefix[256];
    strncpy(prefix, word, prefix_len);
    prefix[prefix_len] = '\0';
    const char *suffix = close + 1;
    
    // extraire le contenu entre accolades
    size_t brace_len = close - open - 1;
    char *brace_content = strndup(open + 1, brace_len);

    // séparer par virgules
    char **res = malloc(64 * sizeof(char *));
    int count = 0;

    char *token = strtok(brace_content, ",");
    while (token) {
        char tmp[512];
        snprintf(tmp, sizeof(tmp), "%s%s%s", prefix, token, suffix);
        res[count++] = strdup(tmp);
        token = strtok(NULL, ",");
    }
    
    // si aucun token trouvé
    if (count == 0) {
        res[0] = strdup(word);
        res[1] = NULL;
        free(brace_content);
        return res;
    }

    res[count] = NULL;
    free(brace_content);

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
                char** resultat = gerer_accolade_mot_simple(mot);
                // vérifie si le mot a été expansé ou est resté identique
                if (resultat[1] != NULL || strcmp(resultat[0], mot) != 0) {
                    // expansion a fonctionné, ajouter les résultats
                    mots_a_traite = ajouter_listes(mots_a_traite, resultat);
                    mots_a_traite = supprimer_element_liste(mots_a_traite, mot_courant);
                } else {
                    // Accolade non fermée, le mot reste identique, passer au suivant
                    mot_courant++;
                }
                // Libérer le résultat temporaire
                for (int j = 0; resultat[j]; j++) free(resultat[j]);
                free(resultat);
            }
            else if (tilde || dollar) {
                wordexp_t w;
                int ret = wordexp(mot,&w,0);
                if (ret != 0){
                    fprintf(stderr, "wordexp failed: %d\n", ret);
                }
                else{
                    int mot_different = 1;
                    for(size_t i=0; i<w.we_wordc; i++){
                        int ret = strcmp(w.we_wordv[i],mot);
                        if(ret == 0){
                            mot_different = 0;
                        }
                    }
                    if(mot_different){
                        mots_a_traite = ajouter_listes(mots_a_traite,w.we_wordv);
                    }
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
    job* prev;
    job* cur;

    // Récupérer tous les fils terminés
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        prev = NULL;
        cur = liste_job;
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

                // Affichage comme dans bash : [n]+ Done    command    temps
                char buf[512];
                int len = snprintf(buf, sizeof(buf), 
                    "\n[%d]+  Done                    %s    (%02ld:%02ld:%02ld:%06ld)\nensishell> ",
                    cur->job_number, cur->nom, hours, minutes, secs, useconds);
                write(STDOUT_FILENO, buf, len);
                
                // supprimer le processus de la liste des jobs
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
                break;
            }
            prev = cur;
            cur = cur->suivant;
        }
    }
}


void initialiser_sigchild(){
     struct sigaction sa;
    memset(&sa, 0, sizeof(sa)); // toujours nettoyer la struct
    sa.sa_handler = handler_childsig;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP; // ignore les stop, relance syscalls interrompus
    sigaction(SIGCHLD, &sa, NULL);
}

void limiter_temps(){
    struct rlimit nouvelle_limite;
    nouvelle_limite.rlim_cur = limite;
    nouvelle_limite.rlim_max = limite + 5;
    setrlimit(RLIMIT_CPU,&nouvelle_limite);
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
        print_jobs();
    }
    else if (strcmp(l->seq[0][0], "ulimit") == 0){
        if(l->seq[0][2] != NULL){
            printf("ulimite prend au max un paramètre \n");
            return;
        }
        int nouvelle_limite = atoi(l->seq[0][1]);
        if(nouvelle_limite == 0){
            printf("ulimite doit etre un nombre positif non nul \n");
            return;
        }
        limite = nouvelle_limite;
        printf("limite fixée a %is \n",limite);
        return;
    }
    else{
        int pid = fork();
        if(pid == 0){
            if(limite != -1) limiter_temps();
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
            waitpid(pid, &wstatus, 0);
        }
    }		
}

void exec_cmd_avec_pipes(struct cmdline* l){
    int nb_cmd = 0;
    while(l->seq[nb_cmd] != NULL) {
        nb_cmd++;
    }
    // variables
    int prev_pipe_out;
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
                dup2(prev_pipe_out,STDIN);
                close(prev_pipe_out);
            }
            //si pas la derniere commande
            if(i < nb_cmd - 1  ){
                // on ecrit dans le pipe avec la sortie standard
                dup2(cur_pipe[1],STDOUT);
                close(cur_pipe[1]);
            }
            
            execvp(l->seq[i][0],l->seq[i]);
            perror("erreur");
            exit(1);
        }	
        if(prev_existe){
            close(prev_pipe_out);
        }
        // Le parent ferme l'extrémité d'écriture qu'il n'utilise jamais
        if(i < nb_cmd - 1){
            close(cur_pipe[1]);
            prev_pipe_out = cur_pipe[0];
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