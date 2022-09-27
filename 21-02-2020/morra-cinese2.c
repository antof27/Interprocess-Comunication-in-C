#include<sys/wait.h>
#include<sys/ipc.h>
#include<sys/msg.h>
#include<ctype.h>
#include<sys/types.h>
#include<stdlib.h>
#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<wait.h>
#include<time.h>
#include<sys/stat.h>
#include<fcntl.h>

#define FIFO_PATH "/home/coloranto/os/21-02-2020/myfifo7"
#define MSG_SIZE sizeof(msg) -sizeof(long)
enum{CARTA, FORBICE, SASSO};
char* mosse[3] = {"carta", "forbice", "sasso"};
enum{P1, P2, J, S};

typedef struct{
    long type; 
    char mossa; 
    char id; 
    char winner; 
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

void player(int queue, char id){
    msg m; 
    m.type = 1; 
    m.done = 0; 
    srand(time(NULL) % getpid());
    while(1){

    if(id == P1){
       // printf("Aspetto che il giudice mi svegli\n");

        if(msgrcv(queue, &m, MSG_SIZE, 2, 0)  <0){
            perror("msgrcv p1");
            exit(1);
        }

        if(m.done) break; 
        
        m.mossa = rand()%3;
        printf("[P1] : mossa %s\n", mosse[m.mossa]);
        
       // printf("Mando la mossa al giudice\n");
        m.type = 1;
        m.id = id; 
        if(msgsnd(queue, &m, MSG_SIZE, 0) == -1){
            perror("msgsnd");
            exit(1);
            }

        } else if(id == P2){
          //  printf("Aspetto che il giudice mi svegli\n");
            if(msgrcv(queue, &m, MSG_SIZE, 2, 0) <0){
                perror("msgrcv p2");
                exit(1);
                }

            if(m.done) break; 
            
            m.mossa = rand()%3;
            printf("[P2] : mossa %s\n", mosse[m.mossa]);
            
          //  printf("Mando la mossa al giudice\n");
            m.type = 1; 
            m.id = id; 
            if(msgsnd(queue, &m, MSG_SIZE, 0) == -1){
                perror("msgsnd");
                exit(1);
                }
        }
    }

    exit(0);

}

int whowins(char mossa1, char mossa2){
    if(mossa1 == mossa2) return 0; 
     if ((mossa1 == CARTA && mossa2 == SASSO) ||
        (mossa1 == FORBICE && mossa2 == CARTA) ||
        (mossa1 == SASSO && mossa2 == FORBICE))
        return 1;
    else return 2;
    }


void judge(int queue, unsigned n_matches){
    msg m; 
    
    unsigned currentMatch = 1; 
    unsigned fakeCurrent = 1; 
    int fifo_des; 

    if((fifo_des = open(FIFO_PATH, O_WRONLY)) == -1){
        perror("open wr fifo");
        exit(1);
    }

    m.type = 2;
    m.done = 0; 

    while(1){
   
    printf("[J] : inizio partita n.%d\n", fakeCurrent);

   // printf("Risveglio il primo processo\n");
    m.type = 2;
    if((msgsnd(queue, &m, MSG_SIZE, 0)) == -1){
        perror("msgsnd judge");
        exit(1);
    }

   // printf("Risveglio il secondo processo\n");
   m.type = 2;
    if((msgsnd(queue, &m, MSG_SIZE, 0)) == -1){
        perror("msgsnd judge");
        exit(1);
    }
     
    char mossap1; 
    char mossap2;

    
    if((msgrcv(queue, &m, MSG_SIZE, 1, 0)) == -1){
        perror("msgrcv j");
        exit(1);
        }
    if(m.id == P1) mossap1 = m.mossa; 
    else mossap2 = m.mossa;
    
    
    if((msgrcv(queue, &m, MSG_SIZE, 1, 0)) == -1){
        perror("msgrcv j");
        exit(1);
        }
    
    if(m.id == P2) mossap2 = m.mossa; 
    else mossap1 = m.mossa;
    

    int winner; 
   // printf("Chiamo winner su : %d e %d\n", mossap1, mossap2); 
    winner = whowins(mossap1, mossap2);
   // printf("winner vale : %d\n", winner);

    if(!winner){
        printf("[J] : partita n.%d patta e quindi da ripetere.\n\n", fakeCurrent);
        fakeCurrent++;
        continue; 
        }

    else if(winner == 1){
        m.winner = 1; 
        printf("[J] : partita n.%d vinta da P1\n", fakeCurrent);
        currentMatch++;
        fakeCurrent++;
        if(write(fifo_des, &winner, sizeof(int)+1) == -1){
            perror("write fifo");
            exit(1);
        }
        
    } 
    else{
        m.winner = 2; 
        printf("[J] : partita n.%d vinta da P2\n", fakeCurrent);
        currentMatch++;
        fakeCurrent++;
        if(write(fifo_des, &winner, sizeof(int)+1) == -1){
            perror("write fifo");
            exit(1);
        }
       
    }

    usleep(100);
    
    int termina = -1;
    if(currentMatch > n_matches) {
        if(write(fifo_des, &termina, sizeof(int)+1) == -1){
            perror("write fifo");
            exit(1);
        }
        break; 
        }
    
   }

    m.done = 1; 
    m.type = 2; 
    if(msgsnd(queue, &m, MSG_SIZE, 0) == -1){
        perror("msgsnd judge");
        exit(1);
        }

    
    if(msgsnd(queue, &m, MSG_SIZE, 0) == -1){
        perror("msgsnd judge");
        exit(1);
     }

    close(fifo_des);
    exit(0);

}

void scoreboard(unsigned n_matches){
    int fifo_des;

    unsigned stats[2] = {0, 0}; 
    int winner; 

    if((fifo_des = open(FIFO_PATH, O_RDONLY)) == -1){
        perror("open fifo");
        exit(1);
    }

   
    while(1){
         
        if(read(fifo_des, &winner, sizeof(int)+1) == -1){
            printf("fifo_d lettura");
            exit(1);
        }
        
        if(winner == -1) break; 

        stats[winner-1]++;
        printf("[S] : classifica temporanea : P1 : %d P2 : %d\n\n", stats[0], stats[1]);
        
    }

    if(read(fifo_des, &winner, 1) == -1){
            printf("fifo_d lettura");
            exit(1);
        }
        stats[winner-1]++;          

        printf("[S] : classifica finale : P1 : %d P2 : %d\n", stats[0], stats[1]);
        int max; 
        
        if(stats[0] > stats[1]) max = 1; 
        else max = 2;

        printf("[S] : il vincitore del torneo è : P%d\n", max);

    close(fifo_des);
    exit(0);

}


int main(int argc, char**argv){
    if(argc !=2){
        fprintf(stderr, "Usage : %s <numero-partite>", argv[0]);
        exit(1);
    }

    int queue = init_queue();
    
    if(mkfifo(FIFO_PATH, 0600) == -1){
        perror("mkfifo");
        exit(1);
    }


    if(!fork()){
        player(queue, P1);
    }

    if(!fork()){
        player(queue, P2);
    }

    if(!fork()){
        judge(queue, atoi(argv[1]));
        }

    if(!fork()){
        scoreboard(atoi(argv[1]));
        }

    wait(NULL);
    wait(NULL);
    wait(NULL);
    wait(NULL);

    unlink(FIFO_PATH);
    msgctl(queue, IPC_RMID, NULL);
}
