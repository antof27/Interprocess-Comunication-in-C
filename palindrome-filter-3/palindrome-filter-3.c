#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/wait.h>
#include<sys/ipc.h>
#include<sys/msg.h>

#define MSG_SIZE_COMPARER sizeof(msg_comparer) - sizeof(long)
#define MSG_SIZE_FATHER sizeof(msg_father) - sizeof(long)
#define MAX_WORD_LEN 50

typedef struct{
    long type;
    char s1[MAX_WORD_LEN];
    char s2[MAX_WORD_LEN];
    unsigned int res; 
    char done; 

}msg_comparer;


typedef struct{
    long type; 
    char word[MAX_WORD_LEN];
    char done; 
}msg_father; 


int init_queue(){
    int queue; 
    if((queue = msgget(IPC_PRIVATE, IPC_CREAT | 0600)) == -1){
        perror("msgget");
        exit(1);
    }
    return queue; 
}

int line_counter(char*path){
    int n_lines = 0; 
    FILE *f; 

if((f = fopen(path, "r")) == NULL){
    perror("fopen");
    exit(1);
}

char buffer[MAX_WORD_LEN];

    while(1){
    if(fgets(buffer, MAX_WORD_LEN, f)){
        n_lines++;
        } else break; 
    }

    return n_lines; 
}

void swap(char**a, char**b){
    char* tmp = *a; 
    *a = *b; 
    *b = tmp; 
}

void sorter(int queue, char*path){
    FILE *f; 
    msg_comparer m_comparer; 
    msg_father m_father; 

    m_comparer.type = 1; 
    m_father.type = 2; 

    if((f = fopen(path, "r")) == NULL){
        perror("fopen");
        exit(1);
    }

    int n_lines = line_counter(path);
    char**buffer = malloc(n_lines*sizeof(char*));

    for(int i = 0; i<n_lines; i++)
        buffer[i] = (char*)malloc(MAX_WORD_LEN+1);
    
        int k = 0; 
        while(fgets(buffer[k], MAX_WORD_LEN, f)){    
            buffer[k][strcspn(buffer[k], "\n")] = '\0';
            k++;
            }

        m_comparer.done = 0; 

        for(int i = 0; i<n_lines; i++)
            for(int j = 0; j<n_lines-i-1; j++){

                sprintf(m_comparer.s1, "%s", buffer[j]);
                sprintf(m_comparer.s2, "%s", buffer[j+1]);
                m_comparer.res = 0; 

                if((msgsnd(queue, &m_comparer, MSG_SIZE_COMPARER, 0)) == -1){
                    perror("msgsnd");
                    exit(1);
                }

            
                if((msgrcv(queue, &m_comparer, MSG_SIZE_COMPARER, 1, 0)) == -1){
                    perror("msgrcv");
                    exit(1);
                }

                if(m_comparer.res){      
                    swap(&buffer[j], &buffer[j+1]);        
                    }

            }

            m_comparer.done = 1;
            if((msgsnd(queue, &m_comparer, MSG_SIZE_COMPARER, 0)) == -1){
                    perror("msgsnd");
                    exit(1);
                }


            
            
            for(int i = 0; i<n_lines; i++){
                sprintf(m_father.word, "%s", buffer[i]);
                m_father.done = 0; 
                if((msgsnd(queue, &m_father, MSG_SIZE_FATHER, 0)) == -1){
                    perror("msgsnd");
                    exit(1);
                }
                
            // printf("--- Stringa inviata : %s\n", m_father.word);
            }

            m_father.done = 1; 
            if((msgsnd(queue, &m_father, MSG_SIZE_FATHER, 0)) == -1){
                    perror("msgsnd");
                    exit(1);
                }
    free(buffer);
    fclose(0);
    exit(0);
}



void comparer(int queue){
    msg_comparer m_comparer; 
     
    while(1){

        memset(&m_comparer, 0, sizeof(msg_comparer));

        if((msgrcv(queue, &m_comparer, MSG_SIZE_COMPARER, 1, 0)) == -1){
            perror("msgrcv");
            exit(1);
            }

        if(m_comparer.done) break; 

        //printf("Stringhe da comparare : %s e %s\n", m_comparer.s1, m_comparer.s2);

        if(strcasecmp(m_comparer.s1, m_comparer.s2) > 0)
            m_comparer.res = 1; 
        
        else m_comparer.res = 0;

        if((msgsnd(queue, &m_comparer, MSG_SIZE_COMPARER, 0)) == -1){
            perror("msgsnd");
            exit(1);
        } 
    }

exit(0);
}

int main(int argc, char**argv){
    
    if(argc != 2){
        fprintf(stderr, "Usage : %s <file1>", argv[0]);
        exit(1);
    }

    int queue = init_queue();
    msg_father m_father; 

    if(!fork()) sorter(queue, argv[1]);

    if(!fork()) comparer(queue);
    
    while(1){

    if((msgrcv(queue, &m_father, MSG_SIZE_FATHER, 2, 0)) == -1){
        perror("msgrcv, questa?"); 
        exit(1);
        }

    
    if(m_father.done) break; 
    
    if(m_father.word != NULL)
    printf("%s\n", m_father.word);

    }
    
    msgctl(queue, IPC_RMID, NULL);
}



