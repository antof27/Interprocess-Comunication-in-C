#define _GNU_SOURCE
#include<unistd.h>
#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<string.h>
#include<sys/wait.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<sys/msg.h>
#include<sys/sem.h>
#include<ctype.h>
#include<fcntl.h>



#define alfa_size 26
#define LINE_SIZE 2048

#define SHM_SIZE sizeof(shm_data)
#define MSG_SIZE sizeof(msg) - sizeof(long)

enum{P, L};

char alfabet[alfa_size] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 
                          'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'};

typedef struct{
    long type; 
    unsigned stats[alfa_size];
    unsigned n_line; 
    char done; 
}msg; 

typedef struct{
    char line[LINE_SIZE];
    char id; 
    unsigned n_line; 
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

int init_queue(){
    int queue; 
    if((queue = msgget(IPC_PRIVATE, IPC_CREAT | 0600)) == -1){
        perror("msgget");
        exit(1);
        return queue; 
    }
}

int init_shm(){
    int shm_des; 
    if((shm_des = shmget(IPC_PRIVATE, SHM_SIZE, IPC_CREAT | 0600))== -1){
        perror("shmget");
        exit(1);
    }

    return shm_des; 
}

int init_sem(){
    int sem_des; 
    
    if((sem_des = semget(IPC_PRIVATE, 2, IPC_CREAT | 0600)) == -1){
        perror("semget");
        exit(1);
    }
    
    if(semctl(sem_des, P, SETVAL, 1)== -1){
        perror("reader setval");
        exit(1);
    }

    if(semctl(sem_des, L, SETVAL, 0)== -1){
        perror("counter setval");
        exit(1);
    }

    return sem_des; 
}

void parent(int sem_des, int shm_des, char*path){
    FILE*f;
    shm_data*data; 

    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("shmat");
        exit(1);
        }

    if((f = fopen(path, "r")) == NULL){
        perror("fopen");
        exit(1);
    }

    char buffer[LINE_SIZE];
    data->n_line = 1; 
    data->done = 0; 

    while(fgets(buffer, LINE_SIZE, f)){
        WAIT(sem_des, P);
        buffer[strcspn(buffer, "\n")] = '\0';
        sprintf(data->line, "%s", buffer);
        printf("[P] : riga n.%d : %s\n", data->n_line, data->line);

        for(int i = 0; i<alfa_size; i++){
            usleep(100);
            SIGNAL(sem_des, L);
            WAIT(sem_des, P);
            }

        data->n_line++;

        SIGNAL(sem_des, P);

    }

    data->done = 1; 
    for(int i = 0; i<alfa_size; i++)
        SIGNAL(sem_des, L);

    fclose(f);
    exit(0);
}

void letter(int sem_des, int shm_des, int queue, char id){
    shm_data*data; 
    unsigned counter[alfa_size];
    msg m;
    char character; 

    
    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("shmat");
        exit(1);
        }

    while(1){
        WAIT(sem_des, L);

        for(int i = 0; i<alfa_size; i++)
            counter[i] = 0;

        data->id = id;  
        m.done = 0;
        int k = 0; 
        
        if(data->done) break; 

        while(1){
            if((character = data->line[k])){
                character = tolower(character);
                counter[character - 'a']++;
                k++;
            } else break;
        }

        for(int i = 0; i<alfa_size; i++)
            m.stats[i] = counter[i];
        
        m.n_line = data->n_line;
        m.type = 1;
        if(msgsnd(queue, &m, MSG_SIZE, 0) == -1){
            perror("msgsnd");
            exit(1);
            }

        WAIT(sem_des, L);
        SIGNAL(sem_des, P);
    }
    
    m.done = 1; 
    if(msgsnd(queue, &m, MSG_SIZE, 0) == -1){
        perror("msgsnd");
        exit(1);
        }

    exit(0);
}


void s_process(int sem_des, int queue){
    msg m; 
    unsigned global_statistics[alfa_size];

    for(int i = 0; i<alfa_size; i++)
        global_statistics[i] = 0; 
     
    while(1){
        for(int i = 0; i<alfa_size; i++){
            if(msgrcv(queue, &m, MSG_SIZE, 1, 0) == -1){
                perror("msgrcv");
                exit(1);
                }
            
        SIGNAL(sem_des, L);
        
            }
        
        if(m.done) break; 
        
        printf("[S] : riga n.%d : ", m.n_line);

        for(int i = 0; i<alfa_size; i++){
            global_statistics[i] += m.stats[i]; 
            if(m.stats[i] != 0)
                printf("%c = %u ", alfabet[i], m.stats[i]);
            }
        printf("\n");
            
        }

    printf("[S] intero file : "); 
    for(int i = 0; i<alfa_size; i++){
        if(global_statistics[i] != 0)
            printf("%c = %u ", alfabet[i], global_statistics[i]);
        }
    
    printf("\n");

    exit(0);
}



int main(int argc, char**argv){
    if(argc<2){
        fprintf(stderr, "Usage : %s <file-1>", argv[0]); 
        exit(1);
    }

    int sem_des, shm_des, queue;

    queue = init_queue(); 
    sem_des = init_sem(); 
    shm_des = init_shm(); 

    
    for(int i = 0; i<alfa_size; i++){
        if(!fork())
            letter(sem_des, shm_des, queue, i);
        }

    if(!fork())
        s_process(sem_des, queue);
        
    parent(sem_des, shm_des, argv[1]);

    for(int i = 0; i<=alfa_size; i++)
        wait(NULL);
        

    semctl(sem_des, 0, IPC_RMID, 0);
    shmctl(shm_des, IPC_RMID, NULL);
    msgctl(queue, IPC_RMID, NULL);

    exit(0);
}
