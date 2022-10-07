#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/wait.h>
#include<sys/ipc.h>
#include<sys/msg.h>
#include<sys/sem.h>
#include<sys/shm.h>
#include<fcntl.h>
#include<sys/types.h>
#include<ctype.h>
#include<sys/mman.h>
#include<sys/stat.h>

#define alfa_size 26
#define SHMC_SIZE sizeof(shm_char)
#define SHMS_SIZE sizeof(shm_stats)

char alfabet[alfa_size] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 
                          'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'};

enum{P, AL, MZ};

typedef struct{
    char letter; 
}shm_char; 

typedef struct{
    long number[alfa_size];
    char done;
}shm_stats;

int WAIT(int sem_des, int numSem){
    struct sembuf op[1] = {{numSem, -1, 0}};
    return semop(sem_des, op, 1);
}
int SIGNAL(int sem_des, int numSem){
    struct sembuf op[1] = {{numSem, +1, 0}};
    return semop(sem_des, op, 1);
}

int init_shmC(){

    int shm_des; 
    if((shm_des = shmget(IPC_PRIVATE, SHMC_SIZE, IPC_CREAT | 0600)) == -1){
        perror("shmgetC");
        exit(1);
    }
    return shm_des;
}

int init_shmS(){

    int shm_des; 
    if((shm_des = shmget(IPC_PRIVATE, SHMS_SIZE, IPC_CREAT | 0600)) == -1){
        perror("shmgetS");
        exit(1);
    }
    return shm_des;
}

int init_sem(){
    int sem_des; 
    if((sem_des = semget(IPC_PRIVATE, 3, IPC_CREAT | 0600)) == -1){
        perror("semget");
        exit(1);
    }

    if((semctl(sem_des, P, SETVAL, 1)) == -1){
        perror("setvalue P");
        exit(1);
    }

    if((semctl(sem_des, AL, SETVAL, 0)) == -1){
        perror("setvalue AL");
        exit(1);
    }

    if((semctl(sem_des, MZ, SETVAL, 0)) == -1){
        perror("setvalue MZ");
        exit(1);
    }
    return sem_des; 
}


void father(int sem_des, int shmc_des, int shms_des, char*path){
    int file, size; 
    struct stat statbuff; 
    char *m;
    shm_char* datac;
    shm_stats* datas;

    if((datac = shmat(shmc_des, NULL, 0)) == NULL){
        perror("shmat datac");
        exit(1);
    }

    if((datas = shmat(shms_des, NULL, 0)) == NULL){
        perror("shmat datas");
        exit(1);
    }

    if((file = open(path, O_RDONLY)) == -1){
        perror("fopen");
        exit(1);
    }

    if((fstat(file, &statbuff)) == -1){
        perror("fstat");
        exit(1);
    }

    if((m = (char*)mmap(NULL, size, PROT_READ, MAP_SHARED, file, 0)) == (char*)-1){
        perror("mmap");
        exit(1);
    }

    size = statbuff.st_size;
    close(file);
    
    datas->done = 0;
    
    for(int i = 0; i<=size; i++){

        WAIT(sem_des, P);

            datac->letter = (char)tolower(m[i]);
            
            if(datac->letter >= 'a' && datac->letter <= 'l')
                SIGNAL(sem_des, AL);
            else if(datac->letter >= 'm' && datac->letter <= 'z')
                SIGNAL(sem_des, MZ);

            else{
                SIGNAL(sem_des, P);
                continue;
            }
        }
                     
    
    datas->done = 1;
    SIGNAL(sem_des, AL);
    SIGNAL(sem_des, MZ);

    printf("---->  Statistiche file  <----\n");
    for(int i = 0; i<alfa_size; i++){
        printf("%c : %ld \n", alfabet[i], datas->number[i]);
    }

    exit(0);

}


void almz(int sem_des, unsigned sem_num, int shms_des, int shmc_des){
    shm_char* datac;
    shm_stats* datas;

    if((datas = shmat(shms_des, NULL, 0)) == NULL){
        perror("shmat datas");
        exit(1);
    }

    if((datac = shmat(shmc_des, NULL, 0)) == NULL){
        perror("shmat datac");
        exit(1);
    }

    while(1){
        WAIT(sem_des, sem_num);
        if(datas->done) break;
        
        datas->number[(int)datac->letter - (int)'a']++;
        

        SIGNAL(sem_des, P);
    }
    exit(0);
}

int main(int argc, char**argv){
    if(argc <2){
        fprintf(stderr, "Usage : %s <file.txt>", argv[0]);
        exit(1);
    }

    int sem_des = init_sem();
    int shmc_des = init_shmC();
    int shms_des = init_shmS();

    
    if(!fork())
        almz(sem_des, AL, shms_des, shmc_des);

    if(!fork())
        almz(sem_des, MZ, shms_des, shmc_des);
    
    father(sem_des, shmc_des, shms_des, argv[1]);

    wait(NULL);
    wait(NULL);

    semctl(sem_des, 0, IPC_RMID);
    shmctl(shms_des, IPC_RMID, NULL);
    shmctl(shmc_des, IPC_RMID, NULL);
}
