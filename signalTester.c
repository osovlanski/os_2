#include "types.h"
#include "stat.h"
#include "user.h"
#include "signal.h"

//#include "x86.h"

void passedTest(int i){
    printf(1,"test #%d is passed\n",i);
}

void failedTest(int i,char *err){
    printf(2,"test #%d is failed. Error:%s\n",i,err);
    exit();
}

void handler(int sig)
{
    printf(1, "handler #1 on user space, signum: %d\n", sig);
}

void handler2(int sig)
{
    printf(1, "handler #2 on user space, signum: %d\n", sig);
}

void restoreActions(){
    for(int i=0; i < 32;i++){
        struct sigaction sa={(void *)SIG_DFL,0};
        sigaction(i, &sa, null);
    }
}
//TODO: add backup for sigmask in kernel signals.


/*
Test1: 
- checking sigaction - insert all the valid/unvalid values
- checking new act is registed and old act is updated
Test2:
- checking all user/kernel signals 
- check signal in mutiple process (father and son)
Test3:
- kill after the proces is stopped (not sleeping)
Test4:
- use only 'custom' kernal signals
- check that all the other signals behavior is like sigkill (not including sigstop,sigcont,sigkill)
Test5:
- kill process that's sleeping (wake him up and kill him)
Task6:
- change sigcont to ignore then do stop/cont/kill
Task7
- 
ToDo: 8/5  
- test sigprocmask, kernel signals and user signals.
- test blocking signals (mask)
- test that at the initail state all handlers act defaultly
- test fork : when a process is being created, using fork(),
 it will inherit the parent’s signal mask and signal handlers, 
 but will not inherit the pending signals. 
 - test exec : return cusotm signal handles to default, except SIG_IGN and SIG_DFL
 
  
 
*/
void test1(){
    //return;
    struct sigaction sa;
    sa.sa_handler = (void *)SIG_DFL;
    sa.sigmask = 0;
    for (int i = 0; i < MAXSIG; i++){
        if (sigaction(i, null, &sa) < 0){  //TODO should not accept null 
            failedTest(1,"sig action failed. couldnot load action to sa");
        }    
        if (sa.sa_handler != SIG_DFL)
            failedTest(1,"init signal handler not working correctly");
    }
    

    sa.sigmask = 123;
    if(sigaction(1, &sa, null) < 0)
        failedTest(1,"sig action failed");
    sa.sa_handler = (void *)SIG_IGN;
    if(sigaction(1, &sa, null) < 0)
        failedTest(1,"sig action failed");
    sa.sa_handler = (void *)SIGKILL;
    if(sigaction(1, &sa, null) < 0)
        failedTest(1,"sig action failed");
    sa.sa_handler = (void *)SIGSTOP;
    if(sigaction(1, &sa, null) < 0)
        failedTest(1,"sig action failed");
    sa.sa_handler = (void *)SIGCONT;
    if(sigaction(1, &sa, null) < 0)
        failedTest(1,"sig action failed");
    sa.sa_handler = handler;
    if(sigaction(1, &sa, null) < 0)
        failedTest(1,"sig action failed");
    struct sigaction sa1;
    if(sigaction(1, &sa, &sa1) < 0)
        failedTest(1,"sig action failed");
    if(!(sa.sa_handler == sa1.sa_handler && sa.sigmask == sa1.sigmask))
        failedTest(1,"sigaction not update oldact");
    sa1.sigmask = 124;
    if(sigaction(1,null,null) == -1)
        failedTest(1,"#0: sigaction should not return -1 on null sigaction");
    
    if(sigaction(-1,&sa,&sa1) != -1) 
        failedTest(1,"#1: sigaction not return -1 on wrong input");
    if(sigaction(32,&sa,&sa1) != -1)
        failedTest(1,"#2: sigaction not return -1 on wrong input");
    if(sigaction(SIGKILL,&sa,&sa1) != -1)
        failedTest(1,"#4: sigaction not return -1 on wrong input");
    if(sigaction(SIGSTOP,&sa,&sa1) != -1)
        failedTest(1,"#5: sigaction not return -1 on wrong input");
    if(sa.sa_handler == sa1.sa_handler && sa.sigmask == sa1.sigmask)
        failedTest(1,"#6: sigaction update although return -1");
    
    restoreActions();
    passedTest(1);
}

void test2(){
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = (void *)SIG_IGN;
    sigaction(SIG_IGN, &sa, null);
    sa.sa_handler = (void *)SIGCONT;
    sigaction(3, &sa, null);
    sa.sa_handler = &handler;
    sigaction(4, &sa, null);
    sa.sa_handler = &handler2;
    sigaction(5, &sa, null);
    
    uint childpid;
    if ((childpid = fork()) == 0)
    {
        printf(1,"child: pid: %d\n",getpid());
        printf(1,"child: stop\n");
        kill(getpid(),SIGSTOP);
        printf(1,"child: continue\n");
        //sleep(10000);
        while(1==1){}
        failedTest(1,"son is suppode do be killed and not run this line");
    }
    else
    {
        printf(1,"father: pid: %d\n",getpid());
        printf(1,"father: ignore signal\n");
        //kill(childpid,SIG_IGN);
        printf(1,"father: send cont to son\n");
        sleep(10);
        kill(childpid,SIGCONT);
    
        sleep(10);
        printf(1,"send 2 user signals to son:\n");

        kill(childpid, 4);
        kill(childpid, 5);
        
        kill(childpid, SIGKILL); //like kill 9 in default
        wait();
    }

    restoreActions();
    passedTest(2);
}

void test3(){
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = (void *)SIG_IGN;
    sigaction(SIG_IGN, &sa, null);
    sa.sa_handler = (void *)SIGCONT;
    sigaction(3, &sa, null);
    sa.sa_handler = &handler;
    sigaction(4, &sa, null);
    sa.sa_handler = &handler2;
    sigaction(5, &sa, null);

    uint childpid;
    if ((childpid = fork()) == 0)
    {
        printf(1,"child: pid: %d\n",getpid());
        printf(1,"child: stop\n");
        kill(getpid(),SIGSTOP);
        sleep(5);
        failedTest(1,"son is suppode do be killed and not run this line");
    }
    else
    {
        printf(1,"father: pid: %d\n",getpid());
        sleep(5);        
        kill(childpid, SIGKILL); //like kill 9 in default
        wait();
    }

    restoreActions();
    passedTest(3);
}

void test4(){
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = (void *)SIG_IGN;
    sigaction(2, &sa, null);
    sa.sa_handler = (void *)SIGSTOP;
    sigaction(3, &sa, null);
    sa.sa_handler = (void *)SIGCONT;
    sigaction(4, &sa, null);
    
    uint childpid;
    if ((childpid = fork()) == 0)
    {
        printf(1,"child: pid: %d\n",getpid());
        printf(1,"child: stop\n");
        kill(getpid(),3);
        printf(1,"child: continue\n");
        while(1==1){}
        failedTest(1,"son is suppode do be killed and not run this line");
    }
    else
    {
        printf(1,"father: pid: %d\n",getpid());
        printf(1,"father: ignore signal\n");
        kill(getpid(),2);
        printf(1,"father: send cont to son\n");
        sleep(10);
        kill(childpid,4);// not suppose to do anythibg 
    
        sleep(10);
        printf(1,"send 2 user signals to son:\n");

        kill(childpid, 10); //like kill 9 in default
        wait();
    }

    restoreActions();
    passedTest(4);
}

void test5(){
    uint childpid;
    if ((childpid = fork()) == 0)
    {
        printf(1,"child: pid: %d\n",getpid());
        sleep(1000000);
    }
    else
    {
        printf(1,"father: pid: %d\n",getpid());
        sleep(50);
        kill(childpid, SIGKILL); //like kill 9 in default
        wait();
    }

    restoreActions();
    passedTest(5);
}


void test6(){
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = (void *)SIG_IGN;
    sigaction(SIGCONT, &sa, null);
    uint childpid;
    if ((childpid = fork()) == 0)
    {
        printf(1,"child: pid: %d\n",getpid());
        sleep(5);
        kill(getpid(), SIGSTOP);
        sleep(5);
        failedTest(1,"son is suppode do be killed and not run this line");
    }
    else
    {
        
        printf(1,"father: pid: %d\n",getpid());
        sleep(50);
        kill(childpid, SIGCONT); //like kill 9 in default
        kill(childpid, SIGKILL); //like kill 9 in default
        wait();
    }

    restoreActions();
    passedTest(6);
}


void test7(){

    if (sigprocmask(1 << SIGCONT) != 0){
        failedTest(7,"ssigprocmask faild");
    }

    if (sigprocmask(1 << SIGCONT) != 1 << SIGCONT){
        failedTest(7,"ssigprocmask faild");
    }

    uint childpid;
    if ((childpid = fork()) == 0)
    {
        printf(1,"child: pid: %d\n",getpid());
        sleep(5);
        kill(getpid(), SIGSTOP);
        sleep(5);
        exit();
        //failedTest(7,"son is not suppose get this lines");
    }
    else
    {
        printf(1,"father: pid: %d\n",getpid());
        sleep(10);
        kill(childpid, SIGCONT);
        sleep(5);
        //kill(childpid, SIGKILL);
        kill(childpid, SIGKILL); //like kill 9 in default
        wait();
    }

    restoreActions();
    passedTest(7);
}


int main(int argc, char *argv[])
{
    // test1();
    // test2();
    // test3();
    // test4();
    test5();
    // test6();
    // test7();
    printf(1,"ALL TESTS PASSED !!!\n");
    exit();
}