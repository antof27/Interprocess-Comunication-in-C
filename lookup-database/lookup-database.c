#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/shm.h>
#include<sys/sem.h>
#include<string.h>
#include<ctype.h>
#include<sys/ipc.h>
#include<sys/wait.h>


#define SHM1_SIZE sizeof(shm_indb)
#define SHM2_SIZE sizeof(shm_dbout)
#define LINE_SIZE 256

enum{DB, OUT, IN};

typedef struct{
    char key[LINE_SIZE];
    char id; 
    char done; 
}shm_indb; 

typedef struct{
    char id; 
    unsigned value; 
    char done; 
}shm_dbout; 

int WAIT(int sem_id, int sem_num) {
    struct sembuf ops[1] = {{sem_num, -1, 0}};
    return semop(sem_id, ops, 1);
}
int SIGNAL(int sem_id, int sem_num) {
    struct sembuf ops[1] = {{sem_num, +1, 0}};
    return semop(sem_id, ops, 1);
}

int init_sem(){
    int sem_des; 

    if((sem_des = semget(IPC_PRIVATE, 3, IPC_CREAT | 0600)) == -1){
        perror("semget");
        exit(1);
        }
    
    if(semctl(sem_des, DB, SETVAL, 0) == -1){
        perror("setval db");
        exit(1);
    }

    if(semctl(sem_des, IN, SETVAL, 1) == -1){
        perror("setval in");
        exit(1);
    }

    if(semctl(sem_des, OUT, SETVAL, 0) == -1){
        perror("setval out");
        exit(1);
    }

    return sem_des; 
}

int init_shm1(){
    int shm_des; 

    if((shm_des = shmget(IPC_PRIVATE, SHM1_SIZE, IPC_CREAT | 0600)) == -1){
        perror("shmget");
        exit(1);
    }
    return shm_des; 
}

int init_shm2(){
    int shm_des; 

    if((shm_des = shmget(IPC_PRIVATE, SHM2_SIZE, IPC_CREAT | 0600)) == -1){
        perror("shmget");
        exit(1);
    }

    return shm_des; 
}

int line_counter(char*path){
    FILE*f; 
    char tmp[LINE_SIZE];
    unsigned n_line; 

    if((f = fopen(path, "r")) == NULL){
        perror("fopen"); 
        exit(1);
    }

    while(fgets(tmp, LINE_SIZE, f)){
        n_line++;
    }

    return n_line;

    exit(0);
}


void in_process(int sem_des, int shm_des, unsigned id, char*path){
    FILE*f; 
    shm_indb* data; 

    if((f = fopen(path, "r")) == NULL){
        perror("fopen db"); 
        exit(1);
    }

    if((data = (shm_indb*)shmat(shm_des, NULL, 0)) == (shm_indb*)-1){
        perror("shmat");
        exit(1);
    }

    data->done = 0; 
    unsigned n_query = 0; 
    
    while(1){
        WAIT(sem_des, IN);

        
        if(fgets(data->key, LINE_SIZE, f)){

        data->key[strcspn(data->key, "\n")] = '\0';
        data->id = id;
        data->done = 0;
        n_query++;
        printf("[IN%d] : inviata query n.%d, '%s'\n", id, n_query, data->key);
        
        SIGNAL(sem_des, DB);

        } else{
            data->done = 1; 
            SIGNAL(sem_des, DB);
            break; 
            }
        }

        fclose(f);
        exit(0);
    }



void db_process(int sem_des, int shm1_des, int shm2_des, char*path, unsigned n_q_process){
    FILE*f; 
    shm_indb* data1; 
    shm_dbout* data2; 

    if((data1 = (shm_indb*)shmat(shm1_des, NULL, 0)) == (shm_indb*)-1){
        perror("shmat");
        exit(1);
    }

    if((data2 = (shm_dbout*)shmat(shm2_des, NULL, 0)) == (shm_dbout*)-1){
        perror("shmat");
        exit(1);
    }

    if((f = fopen(path, "r")) == NULL){
        perror("fopen db"); 
        exit(1);
    }

    unsigned n_lines = line_counter(path);
    char** database = malloc(n_lines* sizeof(char*));

    for(int i = 0; i<n_lines; i++)
        database[i] = (char*) malloc(LINE_SIZE);
    
    for(int i = 0; i<n_lines; i++)
        fgets(database[i], LINE_SIZE, f);
        
        
    
    printf("[DB] : letti %d record da file.\n", n_lines);

    char *tmp;
    char *value; 
    unsigned done_counter = 0; 
    char found = 0; 
    data2->done = 0;
    unsigned value_to = 0;
    while(1){
        WAIT(sem_des, DB);

        if(data1->done){
             done_counter++;
             
             if(done_counter >= n_q_process) 
                break; 
                
             else{
                SIGNAL(sem_des, IN);
                continue; 
             }
        }

        found = 0;
        for(int i = 0; i<n_lines; i++){

            if(strstr(database[i], data1->key) != NULL){
                tmp = strtok(database[i],":");
                value = strtok(NULL, ":");
                value[strcspn(value, "\n")] == '\0';
                value_to = atoi(value);

                printf("[DB] : query '%s' da IN%d trovata con valore : %s", data1->key, data1->id, value);
                found = 1;
                
                data2->id = data1->id;
                data2->value = value_to;
                data2->done = 0; 

                SIGNAL(sem_des, OUT);
                
                break; 
                }  
            }
            if(!found){
                printf("[DB] : query '%s' da IN%d non trovata.\n", data1->key, data1->id);
                SIGNAL(sem_des, IN);    
                }
        }

        //WAIT(sem_des, DB);
        data2->done = 1;
        SIGNAL(sem_des, OUT);

        fclose(f),
        exit(0);
    }


void out_process(int sem_des, int shm_des, unsigned n_queries_process){
    shm_dbout* data; 

    if((data = (shm_dbout*)shmat(shm_des, NULL, 0)) == (shm_dbout*)-1){
        perror("shmat");
        exit(1);
    }

    unsigned global_statistics[n_queries_process][2];

    for(int i = 0; i<n_queries_process; i++)
        for(int j = 0; j<2; j++){
            global_statistics[i][j] = 0; 
        }
    
    while(1){
        WAIT(sem_des, OUT);
        
        if(data->done)
            break;
        
        global_statistics[data->id-1][0]++;
        global_statistics[data->id-1][1]+= data->value;

        SIGNAL(sem_des, IN);
    }

    for(int i = 0; i<n_queries_process; i++){
        printf("[OUT] : ricevuti n.%d valori validi per IN%d con totale : %d\n", 
        global_statistics[i][0], i+1, global_statistics[i][1]);
    }

    exit(0);
}





int main(int argc, char**argv){
    if(argc <3){
        fprintf(stderr, "Usage : %s <db.txt> <query-file1> ... <query-filen>\n", argv[0]);
        exit(1);
    }

    int sem_des = init_sem(); 
    int shm1_des = init_shm1(); 
    int shm2_des = init_shm2(); 

    for(int i = 2; i<argc; i++){
        if(!fork())
            in_process(sem_des, shm1_des, i-1, argv[i]);
            
    }

    if(!fork())
        db_process(sem_des, shm1_des, shm2_des, argv[1], argc-2);

    
    if(!fork())
        out_process(sem_des, shm2_des, argc-2);
    
    for(int i = 0; i < argc; i++)
        wait(NULL);
    

    semctl(sem_des, 0, IPC_RMID, 0);
    shmctl(shm1_des, IPC_RMID, NULL);
    shmctl(shm2_des, IPC_RMID, NULL);
}
