#include<unistd.h>
#include<stdlib.h>
#include<sys/wait.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<sys/msg.h>
#include<sys/sem.h>
#include<string.h>
#include<ctype.h>
#include<stdio.h>


#define alfa_size 26
#define SHM_SIZE sizeof(shm_data)
#define LINE_SIZE 1024
#define MSG_SIZE sizeof(msg) - sizeof(long)

char alfabet[alfa_size] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 
                          'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'};

enum{READER, COUNTER};
typedef struct {
   unsigned id; 
   char line[LINE_SIZE];
   char n_line;
   char done;  
}shm_data;

typedef struct{
long type;
unsigned id;
unsigned n_line;  
unsigned statistics[alfa_size];
char done; 
}msg;

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
    
    if(semctl(sem_des, READER, SETVAL, 1)== -1){
        perror("reader setval");
        exit(1);
    }

    if(semctl(sem_des, COUNTER, SETVAL, 0)== -1){
        perror("counter setval");
        exit(1);
    }

    return sem_des; 
}

void reader(int sem_des, int shm_des, char*path, unsigned id){
    FILE*f; 
    shm_data*data; 

    if((f = fopen(path, "r")) == NULL){
        perror("fopen");
        exit(1);
    }

    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("shmat, cos");
        exit(1);
    }
    data->done = 0;
    unsigned num_line = 1;   

    while(1){
         WAIT(sem_des, READER);
         if(fgets(data->line, LINE_SIZE, f)){

            data->line[strlen(data->line)-1] = '\0';
            data->id = id; 
            data->n_line = num_line; 
            printf("[R%d] : riga-%d : %s\n",data->id, data->n_line, data->line);      
            num_line++;

            SIGNAL(sem_des, COUNTER);
        } else break;

    }
            data->id = id; 
            data->n_line = num_line; 
            data->done = 1; 
            SIGNAL(sem_des, COUNTER);
            

        fclose(f);
        exit(0);
    }


void counter(int sem_des, int shm_des, int queue, unsigned n_readers){
    shm_data*data; 
    msg m; 
    m.type = 1; 
     
    
     
    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("shmat, cos");
        exit(1);
    }

    char line[LINE_SIZE];
    unsigned counter[alfa_size];
    unsigned done_counter = 0; 

    while(1){
        WAIT(sem_des, COUNTER);

        for(int i = 0; i<alfa_size; i++)
            counter[i] = 0;
        
        m.done = 0;
        m.id = data->id; 
        m.n_line = data->n_line;
        

        if(data->done){

            done_counter++;
            data->done = 0; 
            if(done_counter >= n_readers){
                break; 
                }
  
            else {
                m.done =1;

                if((msgsnd(queue, &m, MSG_SIZE,0)) == -1){
                    perror("msgsnd");
                    exit(1);
                    }
                
                SIGNAL(sem_des, COUNTER);
                continue; 
            }
        }

        
        
        sprintf(line, "%s", data->line);
        char character;
        int k = 0; 

        while(1){
            if((character = line[k])){
                character = tolower(character);
                counter[character - 'a']++;
                k++;
                }else break; 
        }

        for(int i = 0; i<alfa_size; i++){
                m.statistics[i] = counter[i];
        }
        
        if((msgsnd(queue, &m, MSG_SIZE,0)) == -1){
            perror("msgsnd");
            exit(1);
        }

        printf("[C] analizzata riga-%d per R%d\n", data->n_line, data->id);

        SIGNAL(sem_des, READER);
    }

    m.done = 1;

    if((msgsnd(queue, &m, MSG_SIZE,0)) == -1){
        perror("msgsnd");
        exit(1);
        }

    exit(0);
}

void parent(int queue, unsigned n_readers){
    msg m; 
    unsigned global_statistics[n_readers][alfa_size];
    unsigned done_counter = 0; 
    m.type = 1; 
    for(int i = 0; i<n_readers; i++)
        for(int j=  0; j<alfa_size; j++)
            global_statistics[i][j] = 0; 
            
    while(1){
        //printf("Pronto a ricevere : \n");
        if((msgrcv(queue, &m, MSG_SIZE, 1, 0)) == -1){
            perror("msgrcv");
            exit(1);
        }
        
        

        if(m.done){
            printf("[P] : statistiche finali su %d righe analizzate per R%d : ", m.n_line, m.id);
            for(int i = 0; i<alfa_size; i++){
                if(global_statistics[m.id-1][i] != 0){
                printf("%c:%d ", alfabet[i], global_statistics[m.id-1][i]);
                }
            }
            printf("\n");
            done_counter++;
           
           
            if(done_counter >= n_readers){
                break; 
            } else {
                usleep(100);
                continue;
            }
        }

        printf("[P] statistica della riga-%d di R%d : ", m.n_line, m.id);

        for(int i = 0; i<alfa_size; i++){
            if(m.statistics[i] != 0){
                printf("%c:%d ", alfabet[i], m.statistics[i]);
                global_statistics[m.id-1][i]++;
            }
        }
        printf("\n");
    }

    exit(0);
}


int main(int argc, char**argv){

    if(argc <2){
        fprintf(stderr, "Usage : %s <file-1> <file-2> ... <file-n>", argv[0]);
        exit(1);
    }

    int shm_des = init_shm();
    int sem_des = init_sem();
    int queue = init_queue();
    unsigned n_readers = argc-1;

    if(!fork()) counter(sem_des, shm_des, queue, n_readers);
    
    for(int i = 1; i<argc; i++){
        if(!fork())
            reader(sem_des, shm_des, argv[i], i);
        }   

    parent(queue, n_readers);

    for(int i = 0; i< argc; i++)
        wait(NULL);

   

    semctl(sem_des, 0, IPC_RMID);
    shmctl(shm_des, IPC_RMID, NULL);   
    msgctl(queue, IPC_RMID, NULL); 

}
