#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<unistd.h>
#include<sys/wait.h>
#include<sys/msg.h>

#define LINE_SIZE 32
#define LINE_NUM 200
#define MSG_SIZE sizeof(msg) - sizeof(long)


typedef struct{
    long type; 
    char line[LINE_SIZE];
    char done; 
}msg; 

int init_queue(){
    int queue; 

    if((queue = msgget(IPC_PRIVATE, IPC_CREAT | 0600)) == -1){
        perror("msgget");
        exit(1);
        }

    return queue; 
}

void reader(int queue, char*path){
    FILE*f; 
    msg m; 

    if((f = fopen(path, "r")) == NULL){
        perror("fopen");
        exit(1);
        }

    char buffer[LINE_SIZE];
    
    while(fgets(buffer, LINE_SIZE, f)){
        buffer[strcspn(buffer, "\n")] = '\0';
        
        if(isspace(buffer[0]))
            memmove(buffer, buffer+1, strlen(buffer));

        buffer[strcspn(buffer, " ")] = '\0';
        sprintf(m.line, "%s", buffer);
   
        m.type = 1;
        m.done = 0;
        if(msgsnd(queue, &m, MSG_SIZE, 0) == -1){
            perror("msgsnd");
            exit(1);
            }

        }
    
    m.done = 1; 
    if(msgsnd(queue, &m, MSG_SIZE, 0) == -1){
        perror("msgsnd");
        exit(1);
        }

    fclose(f);
    exit(0);
    }


    void parent(int queue, int pipefd[2]){
        msg m;
        char collection[LINE_NUM][LINE_SIZE];
        unsigned found = 0; 
        unsigned done_counter = 0; 
        int index = 0; 

        while(1){
            if(msgrcv(queue, &m, MSG_SIZE, 0, 0) == -1){
                perror("msgrcv");
                exit(1);
                }
            
            if(m.done) {
                done_counter++;
                m.done = 0;
                if(done_counter > 1)
                    break; 

                else continue; 
                }
            
            //char tmp[LINE_SIZE];
            found = 0; 
            
            sprintf(collection[index], "%s", m.line);
            index++;

            for(int i = 0; i<index-1; i++){
                if(!strcasecmp(m.line, collection[i])){
                    found = 1; 
                    printf("[P] la parola '%s' mandata è un duplicato\n", m.line);
                    break; 
                    }
                }

                if(!found)
                    dprintf(pipefd[1], "%s\n", m.line); 
            }
        
        exit(0);
    }

    void writer(int pipefd){
        FILE* pipe_f; 

        if((pipe_f = fdopen(pipefd, "r")) == NULL){
            perror("fdopen"),
            exit(1);
        }

        char buffer[LINE_SIZE];
        printf("[W] : \n");

        while(fgets(buffer, LINE_SIZE, pipe_f))
            printf("%s", buffer);
            
        fclose(pipe_f);
        exit(0);
    }


int main(int argc, char**argv){
    if(argc<3){
        fprintf(stderr, "Usage : %s <file1> <file2>\n", argv[0]);
    }

    int queue = init_queue(); 
    int pipefd[2];

    if(pipe(pipefd) == -1){
        perror("pipefd");
        exit(1);
        }

    if(!fork())
        reader(queue, argv[1]);
        

    if(!fork())
        reader(queue, argv[2]);  
         
     if(!fork()){
        close(pipefd[1]);
        writer(pipefd[0]);
        }

    close(pipefd[0]);
    parent(queue, pipefd);

    for(int i = 0; i<3; i++)
        wait(NULL);

    msgctl(queue, IPC_RMID, NULL);
}
