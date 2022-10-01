#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<time.h>
#include<sys/wait.h>
#include<sys/ipc.h>
#include<sys/sem.h>
#include<sys/types.h>
#include<sys/shm.h>

#define SHM_SIZE sizeof(shm_data)
#define LINE_SIZE 1024
enum{J, B};

typedef struct{
    char line[LINE_SIZE];
    unsigned min_value; 
    unsigned max_value;
    unsigned offer; 
    char id; 
    unsigned n_asta; 
    char done; 
}shm_data; 

int WAIT(int sem_id, int sem_num) {
    struct sembuf ops[1] = {{sem_num, -1, 0}};
    return semop(sem_id, ops, 1);
    }

int SIGNAL(int sem_id, int sem_num) {
    struct sembuf ops[1] = {{sem_num, +1, 0}};
    return semop(sem_id, ops, 1);
    }

int init_sem(){
    int sem_des; 

    if((sem_des = semget(IPC_PRIVATE, 2, IPC_CREAT | 0600)) == -1){
        perror("semget");
        exit(1);
        }

    if(semctl(sem_des, J, SETVAL, 1) == -1){
        perror("semctl, setval j");
        exit(1);
        }

    if(semctl(sem_des, B, SETVAL, 0) == -1){
        perror("semctl, setval j");
        exit(1);
        }
        
    return sem_des; 
}

int init_shm(){
    int shm_des; 

    if((shm_des = shmget(IPC_PRIVATE, SHM_SIZE, IPC_CREAT | 0600)) == -1){
        perror("shmget");
        exit(1);
    }

    return shm_des;

}

void judge(int sem_des, int shm_des, char*path, unsigned n_bidders){
    FILE*f; 
    shm_data* data; 

    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("shmat");
        exit(1);
        }

    if((f = fopen(path, "r")) == NULL){
        perror("fopen");
        exit(1);
        }
    
    char buffer[LINE_SIZE];
    char *object;
    char *min_v;
    char *max_v;
    unsigned validOffer, max, emptyAste;
    unsigned validAste = 0;
    unsigned totalAmount = 0; 
    unsigned n_aste_totali = 0; 

    unsigned max_id; 
    data->n_asta = 1; 

    while(fgets(buffer, LINE_SIZE, f)){
        buffer[strcspn(buffer, "\n")] = '\0';
        object = strtok(buffer, ",");
        min_v = strtok(NULL, ",");
        max_v = strtok(NULL, ",");

        validOffer = 0; 
        max = 0; 
        max_id = 0;


        WAIT(sem_des, J);
        
        sprintf(data->line, "%s", object);
        data->min_value = atoi(min_v);
        data->max_value = atoi(max_v);
        data->done = 0;
        

        printf("[J] : lancio asta n.%d per %s con offerta minima di %d EUR e massima di %d EUR\n",
            data->n_asta, data->line, data->min_value, data->max_value);
        
        for(int i = 0; i<n_bidders; i++){
            SIGNAL(sem_des, B);
            WAIT(sem_des, J);

            if(data->offer >= data->min_value && data->offer <= data->max_value){
                validOffer++;
                if(data->offer > max){
                    max = data->offer; 
                    max_id = data->id; 
                    }
                }
            }

            totalAmount+= max;

            if(!validOffer)
                printf("[J] : l'asta n.%d per %s si è conclusa senza alcuna offerta valida pertanto l'oggetto non risulta assegnato.\n",
                  data->n_asta, data->line);

            else validAste++;


        printf("[J] : l'asta n.%d per %s si è conclusa con %d offerte valide su %d\n\n",
        data->n_asta, data->line, validOffer, n_bidders);

        printf("[J] : Il vincitore è B%u che si aggiudica l'oggetto per %d EUR\n\n",
        max_id, max);
        
        data->n_asta++;
        n_aste_totali++;
        SIGNAL(sem_des, J);
        }
    
    emptyAste = n_aste_totali - validAste; 
    printf("[J] : sono state svolte %d aste di cui %d andate assegnate e %d andate a vuoto.\n", n_aste_totali, validAste, emptyAste);
    printf("[J] : il totale raccolto è di %d EUR\n", totalAmount);


    data->done = 1;
    for(int i = 0; i<n_bidders; i++)
        SIGNAL(sem_des, B);

    exit(0);
    }


    void bidder(int sem_des, int shm_des, char id){
    shm_data* data; 
    srand(time(NULL) % getpid());

    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("shmat");
        exit(1);
        }  
    
    while(1){
        WAIT(sem_des, B);
        
        if(data->done) break;
        data->id = id; 
        data->offer = rand()%data->max_value;
        printf("B%d: invio offerta di %d EUR per asta n.%d\n", data->id, data->offer, data->n_asta);
        SIGNAL(sem_des, J);
        }
    exit(0);
    }    



int main(int argc, char**argv){
    if(argc != 3){
        fprintf(stderr, "Usage : %s <auction-file> <num-bidders>\n", argv[0]);
        exit(1);
        }

    int sem_des = init_sem(); 
    int shm_des = init_shm(); 
    unsigned n_bidders = atoi(argv[2]);

    for(int i = 0; i<n_bidders; i++){
        if(!fork())
            bidder(sem_des, shm_des, i+1); 
        }

    

    judge(sem_des, shm_des, argv[1], n_bidders);

     for(int i = 0; i<n_bidders; i++)
     wait(NULL);
    
    semctl(sem_des, 0, IPC_RMID);
    shmctl(shm_des, IPC_RMID, NULL); 

    }
