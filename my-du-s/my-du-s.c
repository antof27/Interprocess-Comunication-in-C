#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/wait.h>
#include<ctype.h>
#include<dirent.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<string.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/sem.h>
#include<linux/limits.h>
#include<sys/msg.h>

enum{SCANNER, STATER};
#define SHM_SIZE sizeof(shm_data)
#define MSG_SIZE sizeof(msg) - sizeof(long)

int WAIT(int sem_id, int sem_num) {
    struct sembuf ops[1] = {{sem_num, -1, 0}};
    return semop(sem_id, ops, 1);
}
int SIGNAL(int sem_id, int sem_num) {
    struct sembuf ops[1] = {{sem_num, +1, 0}};
    return semop(sem_id, ops, 1);
}

typedef struct{
    char path[PATH_MAX];
    char id; 
    char done; 
}shm_data; 

typedef struct{
    long type; 
    unsigned value; 
    char id; 
    char done; 
}msg; 

int init_shm(){
    int shm_des; 

    if((shm_des = shmget(IPC_PRIVATE, SHM_SIZE, IPC_CREAT | 0600)) == -1){
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

    if(semctl(sem_des, SCANNER, SETVAL, 1) == -1){
        perror("setval scanner");
        exit(1);
    }

    if(semctl(sem_des, STATER, SETVAL, 0) == -1){
        perror("setval scanner");
        exit(1);
    }

    return sem_des; 

}

int init_queue(){
    int queue; 
    if((queue = msgget(IPC_PRIVATE, IPC_CREAT | 0600)) == -1){
        perror("msgget");
        exit(1);
    }
    return queue; 
}

void scanner(int sem_des, int shm_des, char id, char *path, char base){
    shm_data* data; 
    DIR*d; 
    struct dirent* dirent; 

    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("shmat");
        exit(1);
    }

    if((d = opendir(path)) == NULL){
        perror("opendir");
        exit(1);
    }
    
    while((dirent = readdir(d))){
        if(!strcmp(dirent->d_name, ".") || !strcmp(dirent->d_name, ".."))
            continue; 
        else if(dirent->d_type == DT_REG){
            WAIT(sem_des, SCANNER);
            data->done = 0; 
            data->id = id; 
            sprintf(data->path,"%s/%s", path, dirent->d_name);
            SIGNAL(sem_des, STATER);
        }
        else if(dirent->d_type == DT_DIR){
            char tmp[PATH_MAX];
            sprintf(tmp, "%s/%s", path, dirent->d_name);
            scanner(sem_des, shm_des, id, tmp, 0);
        }
    }

    closedir(d);

    if(base){
        WAIT(sem_des, SCANNER);
        data->done = 1; 
        SIGNAL(sem_des, STATER);
        exit(0);
    }
}

void stater(int sem_des, int shm_des, int queue, unsigned n_scanners){
    shm_data* data; 
    struct stat statbuf;
    msg m; 
    unsigned done_counter = 0; 
    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("shmat");
        exit(1);
    }

    m.type = 1; 
    m.done = 0; 
    while(1){
        WAIT(sem_des, STATER);
        if(data->done){
            done_counter++;
            if(done_counter >= n_scanners)
                break; 
            else{
                SIGNAL(sem_des, SCANNER);
                continue; 
            }

        }

        if(stat(data->path, &statbuf) == -1){
            perror("stat");
            exit(1);
        }

        m.id = data->id; 
        m.value = statbuf.st_blocks;

        SIGNAL(sem_des, SCANNER);
        
        if((msgsnd(queue, &m, MSG_SIZE, 0)) == -1){
            perror("msgsnd");
            exit(1);
            }
    }

    m.done = 1; 
    if((msgsnd(queue, &m, MSG_SIZE, 0)) == -1){
        perror("msgsnd");
        exit(1);
    }
    exit(0);
}


int main(int argc, char**argv){
    if(argc<2){
        fprintf(stderr, "Usage : %s <path-1> ... <path-n>", argv[0]);
        exit(1);
    }

    int sem_des = init_sem(); 
    int shm_des = init_shm();
    int queue = init_queue(); 
    msg m; 
    unsigned n_scanners = argc-1; 
    unsigned blocks[n_scanners];

    for(int i = 0; i<n_scanners; i++)
        blocks[i] = 0; 

    for(int i = 0; i<n_scanners; i++){
        if(!fork()) 
            scanner(sem_des, shm_des, i, argv[i+1], 1);
        }
    
    if(!fork())
        stater(sem_des, shm_des, queue, n_scanners);

    while(1){
        if(msgrcv(queue, &m, MSG_SIZE, 1, 0) == -1){
            perror("msgrcv");
            exit(1);
        }
        if(m.done) break; 

        blocks[m.id]+= m.value; 

    }

    for (int i = 0; i < n_scanners; i++)
        printf("%u %s\n", blocks[i], argv[i + 1]);

    shmctl(shm_des, IPC_RMID, 0);
    semctl(sem_des, 0, IPC_RMID, 0);
    msgctl(queue, IPC_RMID, 0);

}
