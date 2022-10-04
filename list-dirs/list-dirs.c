#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<ctype.h>
#include<sys/wait.h>
#include<sys/ipc.h>
#include<unistd.h>
#include<sys/shm.h>
#include<sys/sem.h>
#include<sys/stat.h>
#include<dirent.h>
#include<wait.h>
//metal-hellsinger

#define SHM_SIZE sizeof(shm_data)
#define LINE_SIZE 1024

enum{R, F, D};

typedef struct{
    char name[LINE_SIZE];
    unsigned bytes;
    char done; 
}shm_data;

int WAIT(int sem_des, int numSem){
    struct sembuf op[1] = {{numSem, -1, 0}};
    return semop(sem_des, op, 1);
    }

int SIGNAL(int sem_des, int numSem){
    struct sembuf op[1] = {{numSem, +1, 0}};
    return semop(sem_des, op, 1);
    }

int init_sem(){
    int sem_des; 

    if((sem_des = semget(IPC_PRIVATE, 3, IPC_CREAT | 0600)) == -1){
        perror("semget");
        exit(1);
        } 

    if(semctl(sem_des, R, SETVAL, 1) == -1){
        perror("setval r");
        exit(1);
        }   

    if(semctl(sem_des, F, SETVAL, 0) == -1){
        perror("setval file");
        exit(1);
        }

    if(semctl(sem_des, D, SETVAL, 0) == -1){
        perror("setval dir");
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
    FILE* f; 
    shm_data* data; 
    struct stat statbuff; 
    DIR* d; 
    struct dirent* dirent;
    int size; 

    if((d = opendir(path)) == NULL){
        perror("opendir");
        exit(1);
        }

    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("shmat");
        exit(1);
        }

    
    char tmp_path[LINE_SIZE];


        while((dirent = readdir(d))){
                
                data->done = 0; 
               
                if(!strcmp(dirent->d_name, ".") || !strcmp(dirent->d_name, ".."))
                    continue; 
                    
                 
                else if(dirent->d_type == DT_REG){
                    
                    if(path[strcspn(path, "/")])
                        sprintf(tmp_path, "%s%s", path, dirent->d_name);             
                    else sprintf(tmp_path, "%s/%s", path, dirent->d_name);   
                        
                    if((stat(tmp_path, &statbuff)) == -1){
                        perror("statbuff");
                        exit(1);
                        }
                    
                    size = statbuff.st_size;
                    
                    WAIT(sem_des, R);

                    sprintf(data->name, "%s", tmp_path);
                    data->bytes = size;
                    
                    SIGNAL(sem_des, F);
                    
                    }

                else if(dirent->d_type == DT_DIR){

                    if(path[strcspn(path, "/")])
                        sprintf(tmp_path, "%s%s", path, dirent->d_name);             
                    else sprintf(tmp_path, "%s/%s", path, dirent->d_name);   
                    
                    WAIT(sem_des, R);

                    sprintf(data->name, "%s", tmp_path);
                   
                    SIGNAL(sem_des, D);
                    
                    }
            }
        
        WAIT(sem_des, R);
        data->done = 1; 
        SIGNAL(sem_des, D);
        SIGNAL(sem_des, F);

        closedir(d);
        fclose(f);
        exit(0);
    }


void file_consumer(int sem_des, int shm_des, int n_readers){
    shm_data* data;
    unsigned done_counter = 0;
    unsigned n_lettori = n_readers;

    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("shmat");
        exit(1);
        }
    
    while(1){
        WAIT(sem_des, F);
       
        if(data->done){
            done_counter++;
            
            if(done_counter >= n_lettori)
                break;

            else{
                SIGNAL(sem_des, R);
                continue;
                }
            }

        printf("%s [file di %d bytes]\n", data->name, data->bytes);
        
        SIGNAL(sem_des, R);
        
        }

    exit(0);
    }


void dir_consumer(int sem_des, int shm_des, int n_readers){
    shm_data* data;
    unsigned done_counter = 0;
    unsigned n_lettori = n_readers;

    if((data = (shm_data*)shmat(shm_des, NULL, 0)) == (shm_data*)-1){
        perror("shmat");
        exit(1);
        }
    
    while(1){
        WAIT(sem_des, D);
      
        if(data->done){
           done_counter++;            
            
            if(done_counter >= n_lettori) 
                break;
                
            else{
                SIGNAL(sem_des, R);
                continue;
                }
            }

        printf("%s [directory]\n", data->name);
        
        SIGNAL(sem_des, R);

        }
    exit(0);
}


int main(int argc, char **argv){
    
    if(argc <2){
        perror("Usage : %s <dir-1> <dir-n> \n");
        exit(1);
        }

    int sem_des = init_sem();
    int shm_des = init_shm();

    unsigned n_readers = argc-1;

    for(int i = 0; i<n_readers; i++){
        if(!fork())
            reader(sem_des, shm_des, argv[i+1]);
        }

    if(!fork())
        file_consumer(sem_des, shm_des, n_readers);
    
    if(!fork())
        dir_consumer(sem_des, shm_des, n_readers);
    
    for(int i = 0; i<n_readers+2;i++)
        wait(NULL);


    shmctl(shm_des, IPC_RMID, NULL);
    semctl(sem_des, 0, IPC_RMID, 0);

    exit(0);
}
