#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include<sys/wait.h>
#include<sys/ipc.h>
#include<ctype.h>

#define BUF_SIZE 200
#define SHM_SIZE sizeof(shm_data)
enum{MNG, MUL, SUM, SUB};

typedef struct{
    long int n1; 
    long int n2; 
    char eof; 
}shm_data;


int WAIT(int sem_des, int num_semaforo){
struct sembuf operazioni[1] = {{num_semaforo,-1,0}};
return semop(sem_des, operazioni, 1);
    }
int SIGNAL(int sem_des, int num_semaforo){
struct sembuf operazioni[1] = {{num_semaforo,+1,0}};
return semop(sem_des, operazioni, 1);
    }   

int shm_init(){
    int shm_des; 
    if((shm_des = shmget(IPC_PRIVATE, SHM_SIZE, IPC_CREAT | 0600)) == -1){
        perror("shmget");
        exit(1);
    }
    return shm_des;
}

int sem_init(){
    int sem_des; 
    if((sem_des = semget(IPC_PRIVATE,4,  IPC_CREAT | 0600)) == -1){
        perror("semget");
        exit(1);
    }

    if(semctl(sem_des, MNG, SETVAL, 1) == -1){
        perror("setval mng");
        exit(1);
    }

    if(semctl(sem_des, SUB, SETVAL, 0) == -1){
        perror("setval sub");
        exit(1);
    }

    if(semctl(sem_des, MUL, SETVAL, 0) == -1){
        perror("setval mul");
        exit(1);
    }

    if(semctl(sem_des, SUM, SETVAL, 0) == -1){
        perror("setval sum");
        exit(1);
    }

    return sem_des;
}


void manage(int sem_des, int shm_des, char* path){
    FILE *f; 
    if((f = fopen(path, "r")) == NULL){
        perror("fopen");
        exit(1);
    }

    char buffer[BUF_SIZE];
    char buffer2[BUF_SIZE];
    shm_data* data; 
    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("attach");
        exit(1);
    }
    char operation, d; long int operand; 
    data->n1 = 0;

    while(fgets(buffer, BUF_SIZE, f)){
        WAIT(sem_des,MNG);
    
        operation = buffer[0];
        sprintf(buffer2, "%s", buffer+1);
        buffer2[strcspn(buffer2, "\n")] = '\0';
        operand = atoi(buffer2);
     
        
        data->n2 = operand; 
        printf("MNG: risultato intermedio: %ld; letto '%ld'\n", data->n1, data->n2);
        if(operation == '+') SIGNAL(sem_des, SUM);
        if(operation == '*') SIGNAL(sem_des, MUL);
        if(operation == '-') SIGNAL(sem_des, SUB);
    }
    
    WAIT(sem_des, MNG);
    printf("MNG: risultato finale: %ld\n", data->n1);

    data->eof = 1; 
    SIGNAL(sem_des, SUM);
    SIGNAL(sem_des, MUL);
    SIGNAL(sem_des, SUB);
    fclose(0);
    exit(0); 
}

void add(int sem_des, int shm_des){
    
    shm_data* data; 
    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("shmat");
        exit(1);
    }

    long int a, b, c; 

    while(1){
        WAIT(sem_des, SUM);
        if(data->eof) 
            break;
        
        
        a = data->n1; 
        b = data->n2; 
        c = a + b;  
        printf("ADD : %ld + %ld = %ld\n", data->n1, data->n2, c);
        data->n1 = c; 
        SIGNAL(sem_des, MNG);
    }
    
   exit(0);
}

void subtraction(int sem_des, int shm_des){
    shm_data* data; 

    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("shmat");
        exit(1);
    }
    
    long int a, b, c; 
    while(1){
    WAIT(sem_des, SUB);
    if(data->eof) 
        break;
    
    a = data->n1; 
    b = data->n2; 
    c = a - b; 
    printf("SUB : %ld - %ld = %ld\n", data->n1, data->n2, c);
    data->n1 = c; 
    SIGNAL(sem_des, MNG);
    }
    exit(0);
}

void multiplication(int sem_des, int shm_des){
    shm_data* data; 

    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("shmat");
        exit(1);
    }

    long int a, b, c; 
    
    while(1){
        WAIT(sem_des, MUL);
        
        if(data->eof)
            break;
    a = data->n1; 
    b = data->n2; 
    c = a * b; 
    printf("MUL : %ld * %ld = %ld\n", data->n1, data->n2, c);
    data->n1 = c; 
    SIGNAL(sem_des, MNG);
    }
    exit(0);
}


int main(int argc, char**argv){
  
    if(argc != 2){
        fprintf(stderr, "Usage : %s <file1>", argv[0]);
        exit(1);
    }

    int sem_des = sem_init();
    int shm_des = shm_init();

    if(!fork()) manage(sem_des, shm_des, argv[1]);
    if(!fork()) add(sem_des, shm_des);
    if(!fork()) subtraction(sem_des, shm_des);
    if(!fork()) multiplication(sem_des, shm_des);
    
    for (int i = 0; i < 4; i++)
        wait(NULL);

    shmctl(shm_des, IPC_RMID, NULL);
    semctl(sem_des, 0, IPC_RMID);

}
