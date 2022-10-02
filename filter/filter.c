#define _GNU_SOURCE
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/shm.h>
#include<sys/sem.h>
#include<ctype.h>
#include<sys/wait.h>
#include<wait.h>
#include<sys/ipc.h>

#define MAX_LEN 1024
#define SHM_SIZE sizeof(shm_data)

enum{PARENT, FILTER};

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
    if((sem_des = semget(IPC_PRIVATE, 2, IPC_CREAT | 0600)) == -1){
        perror("semget");
        exit(1);
    }

    if(semctl(sem_des, PARENT, SETVAL, 1) == -1){
        perror("setval sem");
        exit(1);
    }

    if(semctl(sem_des, FILTER, SETVAL, 0) == -1){
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

void father(int sem_des, int shm_des, char*path, unsigned n_filters){
    FILE*f; 
    shm_data* data; 

    if((f = fopen(path, "r")) == NULL){
        perror("fopen");
        exit(1);
        }   

    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("shmat"),
        exit(1);
        }
     
     
    char line[MAX_LEN];

    while(fgets(line, MAX_LEN, f)){
            
            line[strcspn(line, "\n")] = '\0';

            WAIT(sem_des, PARENT);
            sprintf(data->word, "%s", line);
            data->done = 0;
            //printf("%s", data->word);

            SIGNAL(sem_des, FILTER);
            WAIT(sem_des, PARENT);

            printf("%s\n", data->word);
            SIGNAL(sem_des, PARENT);
            
        }
   
    data->done = 1;
    for(int i =0; i<n_filters; i++)
        SIGNAL(sem_des, FILTER);


    fclose(f);
    exit(0);
}

void upFilter(char*sentence, char*word){
    
    char*cursor;
    int i = 0;

    while((cursor = strstr(sentence, word)) != NULL){
        i = 0; 
        while(i<strlen(word)){
            cursor[i] = toupper(cursor[i]);
            i++;
            }
        }

}

void downFilter(char*sentence, char*word){
    char*ptr;
    int i = 0;

    while((ptr = strstr(sentence, word)) != NULL){
        i = 0; 
        while(i<strlen(word)){
            ptr[i] = tolower(ptr[i]);
            i++;
            }
        }
    }

void replaceFilter(char*sentence, char*words){
    char*w1;
    char*w2;
    char*ptr;
    char next[MAX_LEN]; 

    w1 = strtok(words, ",");
    w2 = strtok(NULL, ",");

    while((ptr = strstr(sentence, w1)) != NULL){
        strcpy(next, ptr+strlen(w1));
        strcpy(ptr, "");
        strcat(ptr, w2);
        strcat(ptr, next);
    }
}

void filter(int sem_des, int shm_des, char*cmd, unsigned currentFilter, unsigned n_filters){
    shm_data* data;
    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("shmat"),
        exit(1);
        }
    
    char buffer[MAX_LEN];

    while(1){
        WAIT(sem_des, FILTER);
        
        if(data->done) break; 
        
        sprintf(buffer, "%s", data->word);

        if(cmd[0] == '^'){
            upFilter(buffer, cmd+1);
            sprintf(data->word, "%s", buffer);
            }

        else if(cmd[0] == '_'){
            downFilter(buffer, cmd+1);
            sprintf(data->word, "%s", buffer);
            }

        else {  
            char line[MAX_LEN];
            sprintf(line, "%s", cmd+1);
            replaceFilter(buffer, line);
            sprintf(data->word, "%s", buffer); 
            } 
        
        if(currentFilter >= n_filters)
            SIGNAL(sem_des, PARENT);

        else SIGNAL(sem_des, FILTER);
    }
    

    exit(0);
}



int main(int argc, char**argv){
    if(argc< 3){
        fprintf(stderr, "Usage : %s <file.txt> <filter-1> <filter-n>", argv[0]);
        exit(1);
    }

    int sem_des = init_sem();
    int shm_des = init_shm();

    unsigned n_filters = argc-2;

    

    for(int i =0; i<n_filters; i++){
    if(!fork())
        filter(sem_des, shm_des, argv[i+2], i+1, n_filters);
        }
    
    father(sem_des, shm_des, argv[1], n_filters);
    
    for(int i = 0; i<n_filters; i++)
       wait(NULL);

    semctl(sem_des, 0, IPC_RMID, 0);
    shmctl(shm_des, IPC_RMID, NULL);
    exit(0);

}
