#include "types.h"
#include "stat.h"
#include "user.h"
#include "signal.h"

// #define	SIGKILL	9
// #define	SIGSTOP	17	
// #define	SIGCONT	19

// int cpid[5];
// int j;

// void sigcatcher(int signum){
//     printf("PID %d caught process. signal number: %d \n",getpid());
//     if (j > -1){
//         kill(cpid[j],SIGKILL);
//     }
// }


void handler(int sig){
    printf(1,"handler #1 on user space, signum: %d\n",sig);
}

void handler2(int sig){
    printf(1,"handler #2 on user space, signum: %d\n",sig);
}


int
main(int argc, char *argv[])
{
    struct sigaction sa;
    memset(&sa,0,sizeof(sa));
    sa.sa_handler = &handler;
    sigaction(4,&sa,null);
    sa.sa_handler = &handler2;
    sigaction(5,&sa,null);
    kill(getpid(),4);
    kill(getpid(),5);
    
    printf(1,"bye!!!!!!!!!!!!!!!\n");
    exit();
}
