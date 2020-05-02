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

int
main(int argc, char *argv[])
{
    // int n = 5,status,pid;    
    // struct sigaction act = {sigcatcher,0};
    // sigaction(act,)

    // for (size_t i = 0; i < n; i++)
    // {
    //     if((pid = fork()) == pid){
    //         printf(1,"PID %d ready \n",getpid());
    //         j = i-1;

    //     }
    // }
    
    kill(getpid(),9);

    exit();
}
