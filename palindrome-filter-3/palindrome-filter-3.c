#define _GNU_SOURCE
#include<unistd.h>
#include<sys/wait.h>
#include<wait.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<sys/ipc.h>
#include<sys/sem.h>
#include<sys/shm.h>


#define SHM_SIZE sizeof(shm_data)
#define MAX_LEN 64

enum{P, R, W};

typedef struct{
    char word[MAX_LEN];
    unsigned done; 
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

    if((sem_des = semget(IPC_PRIVATE, 3, IPC_CREAT | 0600)) == -1){
        perror("semget");
        exit(1);
    }

    if(semctl(sem_des, R, SETVAL, 0) == -1){
        perror("setval reader");
        exit(1);
    }

     if(semctl(sem_des, P, SETVAL, 0) == -1){
        perror("setval reader");
        exit(1);
    }

     if(semctl(sem_des, W, SETVAL, 0) == -1){
        perror("setval reader");
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

void reader(int sem_des, int shm_des, char*path){
    FILE*f; 
    shm_data* data;

    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("Attach");
        exit(1);
    }

    if(path){
    if((f = fopen(path, "r")) == NULL){
        perror("fopen"); 
        exit(1);
        }
    }
    
    data->done = 0; 
    if(path){
    while(fgets(data->word, MAX_LEN, f)){
        SIGNAL(sem_des, P);
        WAIT(sem_des, R);
        }
    }
    else{
        while(1){
            printf("[R] Leggo da tastiera, 'exit' per terminare\n");
            fgets(data->word, MAX_LEN, stdin);
            if(strcasestr(data->word, "exit") != NULL)
                break; 
            
                SIGNAL(sem_des, P);
                WAIT(sem_des, R);
            
        }
    }

    data->done = 1; 
    SIGNAL(sem_des, P);
    SIGNAL(sem_des, W);

    fclose(f);
    exit(0);
}

int palindrome(char*word){
    int len = strlen(word);

    for(int i = 0, j = len-1; i<=j; i++, j--){
        if(i == j) return 1; 
        else if(tolower(word[i]) != tolower(word[j])){
            
            return 0;  
        }
    }
    return 1; 
}

void father(int sem_des, int shm_des){
    shm_data* data;

    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("Attach");
        exit(1);
    } 
    char buffer[MAX_LEN];

    while(1){
        WAIT(sem_des, P);
        if(data->done) break; 

        sprintf(buffer, "%s", data->word);
        buffer[strlen(buffer)-1] = '\0';
        if(palindrome(buffer)){
            SIGNAL(sem_des, W);
        } else SIGNAL(sem_des, R);

    }

    exit(0);
}

void writer(int sem_des, int shm_des, char*path){
    FILE*f; 
    shm_data* data;

    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("Attach");
        exit(1);
    } 

    if(path){
        if((f = fopen(path, "w")) == NULL){
            perror("Creazione file scrittura");
            exit(1);
            }
       }   

    while(1){
        WAIT(sem_des, W);
        if(data->done) break;
        if(path)
            fputs(data->word, f);
        else printf("[W] palindroma : %s\n", data->word);

        SIGNAL(sem_des, R);
    }

    fclose(f);
    exit(0);

}


int main(int argc, char**argv){
    if(argc<2){
        perror("Argument invalid input");
        exit(1);
    }

    int sem_des = init_sem();
    int shm_des = init_shm(); 

    if(!fork()){
        if(argc > 2)
            writer(sem_des, shm_des, argv[2]);
        else writer(sem_des, shm_des, NULL);
        }


    if(!fork()){
        if(argc > 2)
            reader(sem_des, shm_des, argv[1]);
        else
            reader(sem_des, shm_des, NULL);
        }
    
    father(sem_des, shm_des);

    wait(NULL);
    wait(NULL);

    shmctl(sem_des, IPC_RMID, NULL);
    semctl(sem_des, 0, IPC_RMID, 0);
}
