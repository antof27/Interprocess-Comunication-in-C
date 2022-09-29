#define _GNU_SOURCE
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<stdio.h>
#include<time.h>
#include<sys/shm.h>
#include<sys/sem.h>
#include<sys/wait.h>
#include<sys/ipc.h>
#include<wait.h>
#include<fcntl.h>
#include<sys/types.h>

#define LINE_SIZE 2048
#define SHM_SIZE sizeof(shm_data)
enum{R, W};

typedef struct{
    unsigned id; 
    char line[LINE_SIZE];
    unsigned eof; 
    unsigned found; 
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
        perror("SETVAL R");
        exit(1);
    }

    if(semctl(sem_des, W, SETVAL, 0) == -1){
        perror("SETVAL W");
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

void reader(int sem_des, int shm_des, int pipefd[2], char*path, unsigned n_words){
    FILE*f;
    
    shm_data*data; 
    if((f = fopen(path, "r")) == NULL){
        perror("fopen");
        exit(1);
    }
     
    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("shmat");
        exit(1);
        }

    char buffer[LINE_SIZE];
    
    while(fgets(buffer, LINE_SIZE, f)){

       WAIT(sem_des, R);
       data->found = 0;
       unsigned found_counter = 0; 
       sprintf(data->line, "%s", buffer); 
       data->line[strcspn(data->line, "\n")] = '\0';
        
        for(int i = 0; i<n_words; i++){
            usleep(100); // IMPORTANTISSIMO!
            SIGNAL(sem_des, W);
            WAIT(sem_des, R);
            
            
            if(data->found){
                found_counter++;
                data->found = 0;
                }   
            }      
       
        if(found_counter >= n_words)
            dprintf(pipefd[1], "%s\n", data->line);   

            SIGNAL(sem_des, R);       
        }

    WAIT(sem_des, R);        
    data->eof = 1;
    for(int i = 0; i<n_words; i++)
        SIGNAL(sem_des, W);

    fclose(f);
    
    exit(0);
    }


void worder(int sem_des, int shm_des, char*word, unsigned id){
    shm_data*data; 

    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("shmat");
        exit(1);
        }

    while(1){
        WAIT(sem_des, W);
        
        if(data->eof) break; 

        if((strcasestr(data->line, word))!=NULL){
            data->id = id; 
            data->found = 1;
            } else data->found = 0;

        SIGNAL(sem_des, R);
        }
       
    exit(0);
    }


void outputer(int pipefd){
    char buffer[LINE_SIZE];
    FILE *pipe_f;

	if ((pipe_f = fdopen(pipefd, "r")) == NULL) {
		perror("fdopen O");
		exit(1);
	}
    
    while(fgets(buffer, LINE_SIZE, pipe_f)){
        printf("[O] : %s", buffer);   
        }
         
    fclose(pipe_f);
    exit(0);
}



int main(int argc, char**argv){

    if(argc<3){
        fprintf(stderr, "Usage : %s <file.txt> <word_1> ... <word_n>", argv[0]);
        exit(1);
    }

    int sem_des = init_sem(); 
    int shm_des = init_shm(); 
    int pipefd[2];

    if(pipe(pipefd) == -1){
        perror("pipe");
        exit(1);
    }

     
    if(!fork()){
        close(pipefd[0]);
        reader(sem_des, shm_des, pipefd, argv[1], argc-2);
        }

    for(int i = 0; i<argc-2; i++){
        if(!fork())
            worder(sem_des, shm_des, argv[i+2], i+1);
        }
    
    if(!fork()){
        close(pipefd[1]);
        outputer(pipefd[0]);
        }

    for(int i = 0; i<argc-2; i++)
    wait(NULL);
    
   
    close(pipefd[0]);
    close(pipefd[1]);

    semctl(sem_des, 0, IPC_RMID, 0);
    shmctl(shm_des, IPC_RMID, 0);
    exit(0);

}
