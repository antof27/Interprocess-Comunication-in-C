#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<sys/wait.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<sys/sem.h>
#include<time.h>
#include<sys/types.h>
#include<unistd.h>

enum{P1, P2, G, T};
#define SHM_SIZE sizeof(shm_data)
char* mosse[3] = {"carta", "forbice", "sasso"};
enum{CARTA, FORBICE, SASSO};


typedef struct{
    char mossa_p1;
    char mossa_p2;
    char winner;

    char done; 
}shm_data;


int WAIT(int sem_des, int num_semaforo){
    struct sembuf operazioni[1] = {{num_semaforo,-1,0}};
    return semop(sem_des, operazioni, 1);
    }

int SIGNAL(int sem_des, int num_semaforo){
    struct sembuf operazioni[1] = {{num_semaforo,+1,0}};
    return semop(sem_des, operazioni, 1);
    }

int init_sem(){
    int sem_des; 
    if((sem_des = semget(IPC_PRIVATE, 4, IPC_CREAT | 0600)) == -1){
        perror("semget");
        exit(1);
    }

    if(semctl(sem_des, P1, SETVAL, 1) == -1){
        perror("setval sem");
        exit(1);
    }

    if(semctl(sem_des, P2, SETVAL, 1) == -1){
        perror("setval sem");
        exit(1);
    }

    if(semctl(sem_des, G, SETVAL, 0) == -1){
        perror("setval sem");
        exit(1);
    }

    if(semctl(sem_des, T, SETVAL, 0) == -1){
        perror("setval sem");
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

void player(int sem_des, int shm_des, int sem_num){
    
    shm_data* data; 

    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("shmat"),
        exit(1);
    }

    srand(time(NULL) % getpid());
    data->done = 0;
    while(1){
            if(sem_num == P1){
                WAIT(sem_des, P1);
                if(data->done) break; 
                data->mossa_p1 = rand()%3;
                printf("[P1] : mossa '%s'\n", mosse[data->mossa_p1]);
              
            }
            else if(sem_num == P2){
                WAIT(sem_des, P2);
                if(data->done) break; 
                data->mossa_p2 = rand()%3;
                printf("[P2] : mossa '%s'\n", mosse[data->mossa_p2]);
             
                }
   
            SIGNAL(sem_des, G);      
    }

    exit(0);
}

int whowins(char mossa_p1, char mossa_p2){
    if(mossa_p1 == mossa_p2) return 0; 
    if((mossa_p1 == CARTA && mossa_p2 == SASSO) ||
        (mossa_p1 == SASSO && mossa_p2 == FORBICE)
        || (mossa_p1 == FORBICE && mossa_p2 == CARTA))
        return 1; 
        else return 2; 
}

void judge(int sem_des, int shm_des, unsigned n_matches){
    shm_data* data; 
    unsigned n_match = 0; 
    int winner; 
    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("shmat"),
        exit(1);
    }

    while(1){
        WAIT(sem_des, G);
        WAIT(sem_des, G);
        
        winner = whowins(data->mossa_p1, data->mossa_p2); 
        data->winner = winner; 
        if(!(winner)){
            printf("[G] :partita n.%d patta e quindi da ripetere\n", n_match+1);
            SIGNAL(sem_des, P1);
            SIGNAL(sem_des, P2);
        } else {
            n_match++;
            printf("[G] : partita n.%d vinta da P%d\n", n_match, winner);
            SIGNAL(sem_des, T);
            if(n_match >= n_matches) break; 
            
        }

    }
    exit(0);
}


void scoreboard(int sem_des, int shm_des, unsigned n_matches){
    
    shm_data* data; 
    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("shmat"),
        exit(1);
    }
    char stats[2] = {0, 0};

    for(int i = 0; i<n_matches-1; i++){
        WAIT(sem_des, T);

        stats[data->winner-1]++;
        printf("[T] : classifica temporanea : P1 = %d, P2 = %d\n\n", stats[0], stats[1]);


        SIGNAL(sem_des, P1);
        SIGNAL(sem_des, P2);
    }
        WAIT(sem_des, T);

        stats[data->winner-1]++;
        printf("[T] : classifica finale : P1 = %d, P2 = %d\n", stats[0], stats[1]);

       if(stats[0] > stats[1]) 
            printf("[T] : vincitore del torneo : P1\n");
        else 
            printf("[T] : vincitore del torneo : P2\n");

        data->done = 1; 
        SIGNAL(sem_des, P1);
        SIGNAL(sem_des, P2);

        exit(0);

}

int main(int argc, char**argv){
    if(argc <2){
        fprintf(stderr, "Usage : %s <numero-partite>", argv[0]);
        exit(1);
    }


    int sem_des = init_sem();
    int shm_des = init_shm();

    if(!fork())
        player(sem_des, shm_des, P1);
    
    if(!fork())
        player(sem_des, shm_des, P2);

    if(!fork())
        judge(sem_des, shm_des, atoi(argv[1]));

    if(!fork())
        scoreboard(sem_des, shm_des, atoi(argv[1]));
    
    for(int i =0; i<4; i++)
    wait(NULL);

    semctl(sem_des, 0, IPC_RMID, 0);
    shmctl(shm_des, IPC_RMID, NULL);

}
