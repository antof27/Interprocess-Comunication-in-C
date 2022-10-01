#include<sys/stat.h>
#include<dirent.h>
#include<string.h>
#include<ctype.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/msg.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<fcntl.h>
#include<sys/mman.h>

#define MSG_SIZE sizeof(msg) - sizeof(long)
#define LINE_SIZE 1024

typedef struct{
    long type;
    char command[LINE_SIZE]; 
    unsigned id; 
    char character;
    char file[LINE_SIZE]; 
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


void parent(int queue, char*path, unsigned n_directory){
    msg m; 
    char buffer[LINE_SIZE];
    m.done = 0; 
    char* command;
    char* id_process;
    char *character; 
    char*file;
    unsigned id;
    
    while(1){
        
        printf("file-shell> ");
        fgets(buffer, LINE_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = '\0';
        
        
        if(!strcmp(buffer, "quit")){
            m.done = 1; 
            m.type = 1; 
            for(int i = 0; i<n_directory; i++){
                if(msgsnd(queue, &m, MSG_SIZE, 0) == -1){
                    perror("msgsnd");
                    exit(1);
                    }
                 
                }
            break;
            }

        command = strtok(buffer, " ");
        id_process = strtok(NULL, " ");

        if(!strcmp(command, "num-files") || !strcmp(command, "total-size") || !strcmp(command, "search-char")){

        if(!strcmp(command, "search-char")){
            file = strtok(NULL, " ");
            character = strtok(NULL, " ");

            
            file[strcspn(file, "\n")] = '\0';  
            m.character = character[0]; 
            sprintf(m.file, "%s", file);
        }

        command[strcspn(command, "\n")] = '\0';
        id_process[strcspn(id_process, "\n")] = '\0';        
        id = atoi(id_process);

        sprintf(m.command, "%s", command); 
        m.id = id; 
        m.type = id;  
        m.done = 0; 

        
            if(msgsnd(queue, &m, MSG_SIZE, 0) == -1){
                perror("msgsnd");
                exit(1);
                }
            
            usleep(2000);
        
        }else printf("Comando non corretto, riprova\n");
    }

    exit(0);
}

void directory_process(int queue, char*path, unsigned id){
    msg m; 
    struct stat statbuff; 
    DIR*d; 
    struct dirent* dirent;
    int file;
    char buffer_occurrences[LINE_SIZE];
    char*map; 
    int size; 
   
    while(1){
        unsigned n_regular = 0;
        unsigned n_size = 0; 
        unsigned n_occurrences = 0; 

        if(msgrcv(queue, &m, MSG_SIZE, id, 0) == -1){
            perror("msgrcv");
            exit(1);
            }
        
        if((d = opendir(path)) == NULL){
            perror("opendir");
            }

        if(m.done) break; 

        while(dirent = readdir(d)){
        if(!strcmp(dirent->d_name, ".") || !strcmp(dirent->d_name, ".."))
            continue; 
        else if(dirent->d_type == DT_REG){
            char tmp[LINE_SIZE];
            sprintf(tmp, "%s/%s", path, dirent->d_name);

            if(stat(tmp, &statbuff)){
                perror("apertura file regolare");
                exit(1);
                }
            
            n_size+= statbuff.st_size;
            n_regular++;
        
            if(!strcmp(dirent->d_name, m.file)){
                if((file = open(tmp, O_RDONLY)) == -1){
                    perror("file not opened");
                    exit(1);
                    }

                size = statbuff.st_size;

                if((map = (char*)mmap(NULL, size, PROT_READ, MAP_SHARED, file, 0 )) == (char*)-1){
                    perror("mmap");
                    exit(1);
                    }
                   
                char crctr;
                
                    sprintf(buffer_occurrences,"%s", map);
                   
                    for(int i = 0; i < strlen(buffer_occurrences); i++){
                        crctr = buffer_occurrences[i]; 
                        crctr = tolower(crctr);

                        if(crctr == m.character)
                            n_occurrences++;
                            
                        }
                }
            }
        }

        if(strstr(m.command, "num-files") != NULL){
            printf("%d file\n", n_regular);
            } else if(strstr(m.command, "total-size") != NULL)
                printf("%d byte\n", n_size);
            else printf("%d occurrences \n", n_occurrences);

    }
    munmap(map, size);
    closedir(d);
    exit(0);
}




int main(int argc, char**argv){
    if(argc < 2){
        perror("Usa il n. di parametri corretti");
        exit(1);
    }

    unsigned n_children = argc-1;
    int queue = init_queue(); 

    for(int i = 1; i<=n_children; i++){
        if(!fork())
            directory_process(queue, argv[i], i);
        }

    parent(queue, argv[1], argc-1);

    for(int i = 1; i<=n_children; i++)
        wait(NULL);


    msgctl(queue, IPC_RMID, NULL);
}
