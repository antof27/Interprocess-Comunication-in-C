#define _GNU_SOURCE
#include<wait.h>
#include<sys/wait.h>
#include<sys/msg.h>
#include<sys/ipc.h>
#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<string.h>
#include<sys/types.h>
#include<unistd.h>

#define MSG_SIZE sizeof(msg) - sizeof(long)
#define LINE_SIZE 2048

typedef struct{
    long type;
    char word[LINE_SIZE];
    char done; 
}msg;

int init_queue(){
    int queue_des; 
    if((queue_des = msgget(IPC_PRIVATE, IPC_CREAT | 0600)) == -1){
        perror("msgget");
        exit(1);
    }

    return queue_des; 
}

int lines_counter(char*path){
    int n_lines = 0; 
    FILE*f; 

    if((f = fopen(path, "r")) == NULL){
        perror("fopen");
        exit(1);
    }

    char buffer[LINE_SIZE];
    while(1){
        if(fgets(buffer, LINE_SIZE, f))
        n_lines++;
        else break; 
    }

    fclose(f);
    return n_lines;
}

void reader(int pipefd[2], char*path){
    FILE*f; 
    //FILE* pipe_f;
    unsigned n_lines = lines_counter(path);

    if((f = fopen(path, "r")) == NULL){
        perror("fopen");
        exit(1);
    }
    
   /* if((pipe_f = fdopen(pipefd, "w")) == NULL){
        perror("fdopen pipe");
        exit(1);
    }
    */

    char**buffer = malloc(n_lines * sizeof(char*));
    for(int i = 0; i<n_lines; i++)
        buffer[i] = (char*)malloc(LINE_SIZE+1);
    
    unsigned k = 0; 
    while(1){
        if(fgets(buffer[k], LINE_SIZE, f)){
            buffer[k][strlen(buffer[k])-1] = '\0';

            dprintf(pipefd[1], "%s\n", buffer[k]);
            k++;
            } else break; 
    }

    
    fclose(f);
    exit(0);
    
}

void selectioner(int queue, int pipefd, char*path, char*word){
    FILE* pipe_f;
    msg m; 
    m.type = 1; 
    m.done = 0; 
    if((pipe_f = fdopen(pipefd, "r")) == NULL){
        perror("fdopen pipe");
        exit(1);
    }
    
    char buffer[LINE_SIZE];
    while(fgets(buffer, LINE_SIZE, pipe_f)){
        
            if((strcasestr(buffer, word)) != NULL){
                sprintf(m.word, "%s", buffer); 
            if((msgsnd(queue, &m, MSG_SIZE, 0)) == -1){
                perror("msgsnd");
                exit(1);
                }  
            }

        }

        m.done =1; 
        if((msgsnd(queue, &m, MSG_SIZE, 0)) == -1){
            perror("msgsnd");
            exit(1);
            }
        

            fclose(pipe_f);
            exit(0);
        }
        

void writer(int queue){
    msg m; 
    m.done = 0;
    while(1){
        if((msgrcv(queue, &m, MSG_SIZE, 1, 0)) == -1){
            perror("msgrcv");
            exit(1);
        }

        if(m.done) break;
        printf("[W] :%s", m.word);
    }

    exit(0);
}




int main(int argc, char**argv){
    if(argc<3){
        perror("argc");
        exit(1);
    }

    int queue_des = init_queue();
    int pipefd[2];

    if(pipe(pipefd) == -1){
        perror("pipe creazione");
        exit(1);
        }

    if(!fork()){
        close(pipefd[0]);
        reader(pipefd, argv[2]);
    }

    if(!fork()){
        close(pipefd[1]);
        close(pipefd[0]);
        writer(queue_des);
    }

     
    close(pipefd[1]);
    selectioner(queue_des, pipefd[0], argv[2], argv[1]);

    

    wait(NULL);
    wait(NULL);
    msgctl(queue_des, IPC_RMID, NULL);
    
    exit(0);
}
