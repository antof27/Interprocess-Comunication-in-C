#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/wait.h>
#include<sys/ipc.h>
#include<sys/msg.h>
#include<unistd.h>
#include<sys/types.h>
#include<time.h>

#define MSG_SIZE sizeof(msg) - sizeof(long)

typedef struct{
    long type; 
    unsigned n_match; 
    unsigned mossa; 
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

void judge_parent(int queue, unsigned n_players, unsigned n_matches){
    msg m; 
    m.type = 1; 
    unsigned buffer[n_players];  
    unsigned classifica[n_players];
    m.n_match = 1;  
    unsigned winner, duplicate; 
    unsigned fakeMatch; 

    for(int i = 0; i<n_players; i++)
        classifica[i] = 0;
    
    fakeMatch = 1;

    while(1){    
       
        printf("[J] : inizio partita n.%d\n", fakeMatch);
    
        for(int i = 0; i<n_players; i++)
            buffer[i] = 0;
        
        winner = 0; 
        duplicate = 0; 

        for(int i = 0; i<n_players; i++){
            m.done = 0;
            m.type = 1;
            if(msgsnd(queue, &m, MSG_SIZE, 0) == -1){
                perror("msgsnd");
                exit(1);
                }
            }

        for(int i = 0; i<n_players; i++){
           if(msgrcv(queue, &m, MSG_SIZE, 2, 0) == -1){
                perror("msgrcv");
                exit(1);
                }

            buffer[i] = m.mossa;
            winner += buffer[i];
            }
        
        for(int i = 0; i<n_players; i++){
            for(int j = i+1; j<n_players; j++){
                if(buffer[i] == buffer[j])
                    duplicate = 1; 
                }
            }

        if(duplicate){
            printf("[J] : partita n.%d patta e quindi da ripetere.\n\n", fakeMatch);
            } 

        else{
            winner = winner % n_players; 
            printf("[J] : partita n.%d vinta da P%d.\n\n", fakeMatch, winner);
            m.n_match++;
            classifica[winner]++;
        }

       fakeMatch++;
       if(m.n_match > n_matches) break; 

    }

    printf("[J] : classifica finale : ");
    
    int max = classifica[0]; 
    int index = 0; 
    
    for(int i = 0; i<n_players; i++){
        printf("P%d = %d  ", i, classifica[i]);
        if(classifica[i] > max){
            max = classifica[i];
            index = i; 
            }
        }

    printf("\n");
    printf("[J] : vincitore del torneo : P%d\n", index);
   
    
    
    for(int i = 0; i<n_players; i++){
        m.done = 1; 
        m.type = 1;
        if(msgsnd(queue, &m, MSG_SIZE, 0) == -1){
            perror("msgsnd");
            exit(1);
            }
        }

    exit(0);
}

void player(int queue, char id){
    srand(time(NULL) % getpid());
    msg m; 
    m.done = 0;  
    while(1){
            
        if(msgrcv(queue, &m, MSG_SIZE, 1, 0) == -1){
            perror("msgrcv");
            exit(1);
        }

        
        if(m.done) break; 
        
        m.mossa = rand()%10;
        m.type = 2; 
        
        printf("[P%d] : mossa %d\n", id-2, m.mossa);
   
        if(msgsnd(queue, &m, MSG_SIZE, 0) == -1){
            perror("msgsnd");
            exit(1);
            }
    }

    exit(0);
}


int main(int argc, char**argv){

    if(argc <3){
        perror("Plis metti i parametri giusti");
        exit(1);
    }

    if(atoi(argv[1]) >= 6 || atoi(argv[1]) <= 2){
        perror("Numero giocatori non corretto");
        exit(1);
        }

    if(atoi(argv[2]) < 1){
        perror("Numero partite non corretto");
        exit(1);
    }
    int queue = init_queue();
    unsigned n_players = atoi(argv[1]);
    unsigned n_matches = atoi(argv[2]);

    

    for(int i = 0; i<n_players; i++){
        if(!fork()){
            player(queue, i+2);
            usleep(100);
            }
        }

    judge_parent(queue, n_players,n_matches);
    

    for(int i = 0; i< n_players; i++)
        wait(NULL);

    msgctl(queue, IPC_RMID, NULL);
}
