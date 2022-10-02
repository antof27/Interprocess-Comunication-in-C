#include<stdio.h>
#include<stdlib.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<sys/ipc.h>
#include<sys/msg.h>
#include<string.h>
#include<wait.h>
#include<unistd.h>

#define LINE_SIZE 2048

#define MSGP_SIZE sizeof(msg_p) - sizeof(long)
#define MSGR_SIZE sizeof(msg_r) - sizeof(long)

typedef struct{
    long type; 
    char word[LINE_SIZE];
    char done; 
}msg_p;

typedef struct{
    long type; 
    char word[LINE_SIZE];
    char nome_file[LINE_SIZE];
    unsigned n_linea; 
    unsigned occurrencies; 
    char content_line[LINE_SIZE];
    char done; 
}msg_r;

int init_queue1(){
    int queue; 

    if((queue = msgget(IPC_PRIVATE, IPC_CREAT | 0600)) == -1){
        perror("msgget");
        exit(1);
        }

    return queue; 
}

int init_queue2(){
    int queue; 

    if((queue = msgget(IPC_PRIVATE, IPC_CREAT | 0600)) == -1){
        perror("msgget");
        exit(1);
        }

    return queue; 
}

int line_counter(char*path){
    unsigned n_counter = 0;
    FILE*f;

    if((f = fopen(path, "r")) == NULL){
        perror("fopen");
        exit(1);
        }
    char tmp[LINE_SIZE];

    while(fgets(tmp, LINE_SIZE, f))
        n_counter++;

    fclose(f);
    return n_counter; 
}

void reader(int queue_p, int queue_r, char*path, char id, unsigned n_words){
    FILE*f;
    msg_p mp; 
    msg_r mr; 
    unsigned done_counter = 0; 
    unsigned n_line = line_counter(path);
    char nameFile[LINE_SIZE];
    sprintf(nameFile, "%s", path);

    if((f = fopen(path, "r")) == NULL){
        perror("fopen");
        exit(1);
        }
    
    char** allocated = malloc(n_line * sizeof(char*));

    for(int i = 0; i <n_line; i++)
        allocated[i] = (char*)malloc(LINE_SIZE+1);
    

    for(int i = 0; i<n_line; i++)
        fgets(allocated[i], LINE_SIZE, f); 
    
    for(int i = 0; i<n_line; i++)
        allocated[i][strcspn(allocated[i], "\n")] = '\0';    

    
    while(1){
        
        if(msgrcv(queue_p, &mp, MSGP_SIZE, 1, 0) == -1){
            perror("msgrcv reader");
            exit(1);
            }
        
        if(mp.done) break; 
        
        sprintf(mr.nome_file, "%s", nameFile);
        mr.done = 0;
        mr.type = 2;

        for(int i = 0; i<n_line; i++){
            //printf("riga : %s, parola : %s\n", allocated[i], mp.word);
            if(strstr(allocated[i], mp.word)){
                
                mr.n_linea = i+1;  
                sprintf(mr.content_line, "%s", allocated[i]);
                mr.occurrencies = 1; 
                
                mr.type = 2; 
                if(msgsnd(queue_r, &mr, MSGR_SIZE, 0) == -1){
                    perror("msgsnd reader1");
                    exit(1);
                    }
                }
            }

            mr.done = 1;
            mr.type = 2; 
            if(msgsnd(queue_r, &mr, MSGR_SIZE, 0) == -1){
                perror("msgsnd reader2");
                exit(1);
                }       
            }

    
    fclose(f);
    exit(0);

}

void parent(int queue_p, int queue_r, char** buffer_words, unsigned n_words, unsigned n_readers){
    msg_p mp; 
    msg_r mr;
    unsigned n_lettori = n_readers;
    char nome_files[n_lettori][LINE_SIZE];
    unsigned total_occurences[n_lettori][1];

    for(int i = 0; i<n_lettori; i++)
        for(int j = 0; j<1; j++)
            total_occurences[i][j] = 0;
        
        unsigned done_counter_file = 0;
        int k = 0;

        while(1){
            
            buffer_words[k][strcspn(buffer_words[k], "\n")] = '\0';
            sprintf(mp.word, "%s", buffer_words[k]);
            
                mp.done = 0; 
                mp.type = 1;
                
                for(int i = 0; i<n_lettori; i++){
                    if(msgsnd(queue_p, &mp, MSGP_SIZE, 0) == -1){
                        perror("msgsnd parent");
                        exit(1);
                        }
                    }

                done_counter_file = 0;

                while(1){
                    
                    if(msgrcv(queue_r, &mr, MSGR_SIZE, 0, 0) ==  -1){
                        perror("msgrcv parent");
                        exit(1);
                        }
                 
                    if(mr.done){
                        done_counter_file++;       
                        if(done_counter_file >= n_lettori){
                           
                            break;
                            }

                        else continue;

                        }

                    sprintf(nome_files[k], "%s", mr.nome_file);

                    if(mr.occurrencies) total_occurences[k][0]++;

                    printf("%s@%s:%d:%s\n", mp.word, mr.nome_file, mr.n_linea, mr.content_line);
                    
                    }
                k++;

                if(k >= n_words)
                    break;
                 
            }
       
        for(int i = 0;i<n_words; i++)
            printf("\n%s:%u", nome_files[i], total_occurences[i][0]);

        
        mp.done = 1; 
        for(int i = 0; i<n_lettori; i++){
            mp.type = 1;
            if(msgsnd(queue_p, &mp, MSGP_SIZE, 0) == -1){
                perror("msgsnd padre");
                exit(1);
                }
            }

    exit(0);
    }



int main(int argc, char**argv){
    if(argc < 4){
        fprintf(stderr, "Usage : %s <word-1> [word-2] @ <file-1> [file-2]\n", argv[0]);
        exit(1);
        }  

    unsigned n_words = 0;
    unsigned n_readers = 0; 

    int queue_p = init_queue1(); 
    int queue_r = init_queue2(); 

    char*ptr; 
    char line[LINE_SIZE];
    int i = 0; 
    
    for(int i = 1; i<argc; i++){
        if(strcmp(argv[i], "@"))
            n_words++;
        else break;
        }

    n_readers = (argc-2) - n_words; 

    char** words_buffer = malloc(n_words * sizeof(char*));
    for(int i = 0; i<n_words; i++)
        words_buffer[i] = (char*)malloc(LINE_SIZE+1);

    for(int i = 0; i<n_words; i++)
        sprintf(words_buffer[i], "%s", argv[i+1]);
        
    

    for(int i = 0; i<n_readers; i++){
        if(!fork())
            reader(queue_p, queue_r, argv[(n_words+2)+i], i, n_words);
            
        }

    parent(queue_p, queue_r, words_buffer, n_words, n_readers);

    for(int i = 0; i<n_readers; i++)
        wait(NULL);

    msgctl(queue_p, IPC_RMID, NULL);
    msgctl(queue_r, IPC_RMID, NULL);

    exit(0);


}
