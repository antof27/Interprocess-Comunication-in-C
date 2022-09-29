#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/wait.h>
#include<sys/ipc.h>
#include<sys/types.h>
#include<unistd.h>
#include<sys/mman.h>
#include<ctype.h>
#include<fcntl.h>
#include<sys/stat.h>

#define LINE_SIZE 64

void reader(int pipefd[2], char*path){
    int f; 
    char*m; 
    int size = 0; 
    struct stat statbuff;


    if((f = open(path, O_RDONLY)) == -1){
        perror("open file");
        exit(1);
        }

    if(fstat(f, &statbuff) == -1){
            perror("fstat");
            exit(1);
        }
    
    size = statbuff.st_size;

    if((m = (char*)mmap(NULL,size, PROT_READ, MAP_SHARED, f,0)) == (char*)-1){
        perror("mmap");
        exit(1);
    }

    dprintf(pipefd[1], "%s", m);

    munmap(m, size);
    close(f);
    exit(0);
    }

int palindrome_checker(char*word){
    int len = strlen(word);

    for(int i = 0, j = len-1; i<=j; i++, j--){
        if(i == j) return 1;
        else if(tolower(word[i]) != tolower(word[j]))
            return 0;
        }
        return 1;
    }

void parent(int pipefd, int pipefd2[1]){
    FILE* pipe_f; 

    if((pipe_f = fdopen(pipefd, "r")) == NULL){
        perror("fdopen pipe");
        exit(1);
    }

    char buffer[LINE_SIZE];

    


    while(fgets(buffer, LINE_SIZE, pipe_f)){
            buffer[strcspn(buffer, "\n")] = '\0';
           // printf("[P] : %s\n", buffer);
        if(palindrome_checker(buffer)){
            //printf("Scrivo nell'output : %s\n", buffer);
            dprintf(pipefd2[1], "%s\n", buffer);
            }
        }

    fclose(pipe_f);

    exit(0);
}


void writer(int pipefd2){
    FILE* pipe_f2; 
    
    if((pipe_f2 = fdopen(pipefd2, "r")) == NULL){
        perror("pipefd pipe");
        exit(1);
        }

    char buffer[LINE_SIZE];

    while(fgets(buffer, LINE_SIZE, pipe_f2)){
        printf("[W] : %s", buffer);
        } 
    
    fclose(pipe_f2);
    close(pipefd2);
    exit(0);
}


int main(int argc, char**argv){
    
    if(argc < 2){
        fprintf(stderr, "Usage : %s <input-file>", argv[0]);
        exit(1);
    }

    int pipefd[2]; 
    int pipefd2[2]; 

    if(pipe(pipefd) == -1){
        perror("pipe");
        exit(1);
    }
    
    if(pipe(pipefd2) == -1){
        perror("pipe");
        exit(1);
    }

    

    if(!fork()){
        close(pipefd2[0]);
        close(pipefd2[1]);
        close(pipefd[0]);
        reader(pipefd, argv[1]);
        } 

    if(!fork()){
        close(pipefd[0]);
        close(pipefd[1]);
        close(pipefd2[1]);
        writer(pipefd2[0]);
        }

    close(pipefd[1]);
    close(pipefd2[0]);
    parent(pipefd[0], pipefd2);
    

  
    
    
    wait(NULL);
    wait(NULL);

    exit(0);
}
