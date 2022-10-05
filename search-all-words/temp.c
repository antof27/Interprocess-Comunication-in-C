#define _GNU_SOURCE
#include<unistd.h>
#include<stdio.h>
#include<string.h>
#include<sys/wait.h>
#include<wait.h>
#include<sys/ipc.h>
#include<stdlib.h>
#include<sys/shm.h>
#include<sys/sem.h>
#include<sys/types.h>
#include<ctype.h>

#define SHM_SIZE sizeof(shm_data)
#define LINE_SIZE 1024

enum{R, W};

typedef struct{
    char line[LINE_SIZE];
    unsigned found;  
    char done; 
}shm_data;

int WAIT(int sem_des, int num_semaforo){
    struct sembuf operazioni[1] = {{num_semaforo,-1,0}};
    return semop(sem_des, operazioni, 1);}
    
int SIGNAL(int sem_des, int num_semaforo){
    struct sembuf operazioni[1] = {{num_semaforo,+1,0}};
    return semop(sem_des, operazioni, 1);}

int init_sem(){
    int sem_des; 

    if((sem_des = semget(IPC_PRIVATE, 2, IPC_CREAT | 0600)) == -1){
        perror("semget");
        exit(1);
        }
    
    if(semctl(sem_des, R, SETVAL, 1) == -1){
        perror("semctl");
        exit(1);
        }

    if(semctl(sem_des, W, SETVAL, 0) == -1){
        perror("semctl");
        exit(1);
        }

    return sem_des;
    exit(0);
    }

int init_shm(){
    int shm_des; 

    if((shm_des = shmget(IPC_PRIVATE, SHM_SIZE, IPC_CREAT | 0600)) == -1){
        perror("shmget");
        exit(1);
        }

    return shm_des;
}


void reader(int sem_des, int shm_des, int pipefd[2], char*path, unsigned n_words){
    
    shm_data*data;
    FILE*f; 
    unsigned found_counter;

    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("shmat");
        exit(1);
        }

    if((f = fopen(path, "r")) == NULL){
        perror("fopen");
        exit(1);
        }
    
    char buffer[LINE_SIZE];
    data->done = 0; 

    while(fgets(buffer, LINE_SIZE, f)){
        buffer[strcspn(buffer, "\n")] = '\0';

        WAIT(sem_des, R);
        sprintf(data->line, "%s", buffer);
        
        found_counter = 0;
        data->found = 0;
        for(int i = 0; i<n_words; i++){
            usleep(100);
            SIGNAL(sem_des, W);
            WAIT(sem_des, R);

            if(data->found){ 
                found_counter++;
                data->found = 0;
                }
            }
            

        if(found_counter >= n_words)
            dprintf(pipefd[1], "%s", data->line);
            
        
        SIGNAL(sem_des, R);
        }

    WAIT(sem_des, R);
    data->done = 1;
    for(int i = 0; i<n_words; i++)
        SIGNAL(sem_des, W);


    fclose(f);
    exit(0);
}

void writer(int sem_des, int shm_des, char*word){
    shm_data*data; 

    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("shmat");
        exit(1);
        }

    
    while(1){
        WAIT(sem_des, W);
        
        if(data->done) break; 

        if((strcasestr(data->line, word)) != NULL){
            data->found = 1;
            } else data->found = 0;
        
        SIGNAL(sem_des, R);
    }

    exit(0);
}


void output(int pipefd){
    FILE* pipe_f;

    if((pipe_f = fdopen(pipefd,"r" )) == NULL){
        perror("fdopen");
        exit(1);
        }

    char line[LINE_SIZE];
    while(fgets(line, LINE_SIZE, pipe_f))
        printf("[O] : %s\n", line);

    exit(0);
}


int main(int argc, char**argv){
    
    if(argc < 3){
        fprintf(stderr, "Usage : %s <text-file> <word-1> [word-n]\n", argv[0]);
        exit(1);
        }

    int shm_des = init_shm();
    int sem_des = init_sem();
    
    int pipefd[2];
    unsigned n_words = argc-2;

    if(pipe(pipefd) == -1){
        perror("pipe");
        exit(1);
        }
    
    if(!fork()){
        close(pipefd[0]);
        reader(sem_des, shm_des, pipefd, argv[1], n_words);
        }

    for(int i = 0; i<n_words; i++){
        if(!fork())
            writer(sem_des, shm_des, argv[i+2]);
            }        

    if(!fork()){
        close(pipefd[1]);
        output(pipefd[0]);
        }

    for(int i = 0; argc-1; i++)
        wait(NULL);

    close(pipefd[1]);
    semctl(sem_des, 0, IPC_RMID, 0);
    shmctl(shm_des, IPC_RMID, NULL);
    exit(0);
}
