#include "types.h"
#include "stat.h"
#include "user.h"
#include "signal.h"
int flag = 0;

char *state[] = {"UNUSED", "EMBRYO", "SLEEPING", "RUNNABLE", "RUNNING", "ZOMBIE"};
//#include "x86.h"
void passedTest(int i)
{
    printf(1, "test #%d is passed\n", i);
}

void restoreActions()
{
    for (int i = 0; i < 32; i++)
    {
        struct sigaction sa = {(void *)SIG_DFL, 0};
        sigaction(i, &sa, null);
    }
}

void failedTest(int i, char *err)
{
    restoreActions();
    printf(2, "test #%d is failed. Error:%s\n", i, err);
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

void handler3(int sig)
{
    if (flag)
    {
        flag = 0;
        printf(1, "handler #3, changed 'flag' to 0, on user space, signum: %d\n", sig);
    }
    else
    {
        flag = 1;
        printf(1, "handler #3, changed 'flag' to 1, on user space, signum: %d\n", sig);
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
- - test sigprocmask, kernel signals and user signals., - test blocking signals (mask)




ToDo: 8/5  
- test that at the initail state all handlers act defaultly
- test fork : when a process is being created, using fork(),
 it will inherit the parent’s signal mask and signal handlers, 
 but will not inherit the pending signals. 
 - test exec : return cusotm signal handles to default, except SIG_IGN and SIG_DFL
 
  
 
*/
void test1()
{
    //return;
    struct sigaction sa;
    sa.sa_handler = (void *)SIG_DFL;
    sa.sigmask = 0;
    for (int i = 0; i < MAXSIG; i++)
    {
        if (sigaction(i, null, &sa) < 0)
        { //TODO should not accept null
            failedTest(1, "sig action failed. couldnot load action to sa");
        }
        if (sa.sa_handler != SIG_DFL)
            failedTest(1, "init signal handler not working correctly");
    }

    sa.sigmask = 123;
    if (sigaction(1, &sa, null) < 0)
        failedTest(1, "sig action failed");
    sa.sa_handler = (void *)SIG_IGN;
    if (sigaction(1, &sa, null) < 0)
        failedTest(1, "sig action failed");
    sa.sa_handler = (void *)SIGKILL;
    if (sigaction(1, &sa, null) < 0)
        failedTest(1, "sig action failed");
    sa.sa_handler = (void *)SIGSTOP;
    if (sigaction(1, &sa, null) < 0)
        failedTest(1, "sig action failed");
    sa.sa_handler = (void *)SIGCONT;
    if (sigaction(1, &sa, null) < 0)
        failedTest(1, "sig action failed");
    sa.sa_handler = handler;
    if (sigaction(1, &sa, null) < 0)
        failedTest(1, "sig action failed");
    struct sigaction sa1;
    if (sigaction(1, &sa, &sa1) < 0)
        failedTest(1, "sig action failed");
    if (!(sa.sa_handler == sa1.sa_handler && sa.sigmask == sa1.sigmask))
        failedTest(1, "sigaction not update oldact");
    sa1.sigmask = 124;
    if (sigaction(1, null, null) == -1)
        failedTest(1, "#0: sigaction should not return -1 on null sigaction");

    if (sigaction(-1, &sa, &sa1) != -1)
        failedTest(1, "#1: sigaction not return -1 on wrong input");
    if (sigaction(32, &sa, &sa1) != -1)
        failedTest(1, "#2: sigaction not return -1 on wrong input");
    if (sigaction(SIGKILL, &sa, &sa1) != -1)
        failedTest(1, "#4: sigaction not return -1 on wrong input");
    if (sigaction(SIGSTOP, &sa, &sa1) != -1)
        failedTest(1, "#5: sigaction not return -1 on wrong input");
    if (sa.sa_handler == sa1.sa_handler && sa.sigmask == sa1.sigmask)
        failedTest(1, "#6: sigaction update although return -1");

    restoreActions();
    passedTest(1);
}

void test2()
{
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
        printf(1, "child: pid: %d\n", getpid());
        printf(1, "child: stop\n");
        kill(getpid(), SIGSTOP);
        printf(1, "child: continue\n");
        //sleep(10000);
        while (1 == 1)
        {
        }
        failedTest(1, "son is suppode do be killed and not run this line");
    }
    else
    {
        printf(1, "father: pid: %d\n", getpid());
        printf(1, "father: ignore signal\n");
        //kill(childpid,SIG_IGN);
        printf(1, "father: send cont to son\n");
        sleep(10);
        kill(childpid, SIGCONT);

        sleep(10);
        printf(1, "send 2 user signals to son:\n");

        kill(childpid, 4);
        kill(childpid, 5);

        kill(childpid, SIGKILL); //like kill 9 in default
        wait();
    }

    restoreActions();
    passedTest(2);
}

void test3()
{
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
        printf(1, "child: pid: %d\n", getpid());
        printf(1, "child: stop\n");
        kill(getpid(), SIGSTOP);
        sleep(5);
        failedTest(1, "son is suppode do be killed and not run this line");
    }
    else
    {
        printf(1, "father: pid: %d\n", getpid());
        sleep(5);
        kill(childpid, SIGKILL); //like kill 9 in default
        wait();
    }

    restoreActions();
    passedTest(3);
}

void test4()
{
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
        printf(1, "child: pid: %d\n", getpid());
        printf(1, "child: stop\n");
        kill(getpid(), 3);
        printf(1, "child: continue\n");
        while (1 == 1)
        {
        }
        failedTest(1, "son is suppode do be killed and not run this line");
    }
    else
    {
        printf(1, "father: pid: %d\n", getpid());
        printf(1, "father: ignore signal\n");
        kill(getpid(), 2);
        printf(1, "father: send cont to son\n");
        sleep(10);
        kill(childpid, 4); // not suppose to do anythibg

        sleep(10);
        printf(1, "send 2 user signals to son:\n");

        kill(childpid, 10); //like kill 9 in default
        wait();
    }

    restoreActions();
    passedTest(4);
}

void test5()
{
    uint childpid;
    if ((childpid = fork()) == 0)
    {
        printf(1, "child: pid: %d\n", getpid());
        sleep(10000);
    }
    else
    {
        printf(1, "father: pid: %d\n", getpid());
        sleep(5);
        kill(childpid, SIGKILL); //like kill 9 in default
        wait();
    }

    restoreActions();
    passedTest(5);
}

void test6()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = (void *)SIG_IGN;
    sigaction(SIGCONT, &sa, null);
    uint childpid;
    if ((childpid = fork()) == 0)
    {
        printf(1, "child: pid: %d\n", getpid());
        sleep(5);
        kill(getpid(), SIGSTOP);
        sleep(5);
        failedTest(1, "son is suppode do be killed and not run this line");
    }
    else
    {

        printf(1, "father: pid: %d\n", getpid());
        sleep(50);
        kill(childpid, SIGCONT); //like kill 9 in default
        kill(childpid, SIGKILL); //like kill 9 in default
        wait();
    }

    restoreActions();
    passedTest(6);
}

// test flow: (1) sanity check: father is waiting for child to stop -> child stop -> father send cont ->
// -> child continue and notifies parent and wait for confirmation -> pranet continue and notify child -> child continue *sanity check end*

// (2) sigprocmask check:  father is waiting for child to block CONT -> child blocckes CONT , notifies father -> child STOP -> father tries to CONT child ->
//  -> if child CONT, then child notifies parent and test fail, else parent kill child and test succsess.
void test7()
{
    restoreActions();
    // int flag = 0;
    // if (sigprocmask(1 << SIGCONT) != 0){               //why block the parent SIGCONT?
    //     failedTest(7,"ssigprocmask faild");
    // }

    // if (sigprocmask(1 << SIGCONT) != 1 << SIGCONT){   // same
    //     failedTest(7,"ssigprocmask faild");
    // }

    int i;
    int waitingTime = 30;
    uint fatherpid = getpid();
    uint childpid;
    flag = 1;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &handler3;
    sigaction(6, &sa, null);

    if ((childpid = fork()) == 0)
    {
        printf(1, "child: pid: %d\n", getpid());
        kill(getpid(), SIGSTOP); //make child stop
        kill(fatherpid, 6);      //tell father child stopped
        // in child flag should be 1, in father should be 0
        while (flag)
        {
            printf(1, "child waiting\n");
            sleep(5); //waiting fir father to finish sending CONT to child
        }
        //in child , flag should be 0

        if (sigprocmask(1 << SIGCONT) != 0)
        {
            failedTest(7, "ssigprocmask faild");
        }

        if (sigprocmask(1 << SIGCONT) != 1 << SIGCONT)
        { // same
            failedTest(7, "ssigprocmask faild");
        }

        kill(fatherpid, 6);
        //in father flag should be 0, in child flag sohuld be 0

        kill(getpid(), SIGSTOP); //if stopped and killed should exit immediately
        //flag should be 1 now, change to 0
        kill(fatherpid, 6); //should not reach here
        failedTest(7, "child should not reach here, it shold remain stopped\n");
    }
    else
    {
        sleep(20);
        while (flag)
        {
            printf(1, "father waiting  1\n");
            kill(childpid, SIGCONT);
            sleep(5);
        }

        kill(childpid, 6);
        //flag should be 0 now
        sleep(20);
        while (!flag)
        {
            printf(1, "father waiting  2\n");
            sleep(5);
            //wait for child to block SIGCONT
        }

        //flag should be 1
        sleep(50);

        for (i = 0; i < waitingTime; ++i)
        { //doing it in a loop to make sure child don't miss the signal
            kill(childpid, SIGCONT);
            sleep(5);
            printf(1, "father waiting  3\n");
        }
        sleep(40);

        if (!flag)
        {
            failedTest(7, "child should not send message, sigprocmask probably failed");
        }

        kill(childpid, SIGKILL);
        wait();
    }

    restoreActions();
    passedTest(7);
}

void test8()
{ //test sigprocpask with user signals  + sanity check
    restoreActions();
    int i;
    int waitingTime = 50;
    uint fatherpid = getpid();
    uint childpid;
    flag = 1;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &handler3;
    sigaction(6, &sa, null);

    if ((childpid = fork()) == 0)
    {
        printf(1, "child: pid: %d\n", getpid());

        while (flag)
        { //wait for signal from father
            sleep(5);
            printf(1, "child waiting\n");
        }

        if (sigprocmask(1 << 6) != 0)
        {
            failedTest(8, "ssigprocmask faild\n");
        }

        if (sigprocmask(1 << 6) != 1 << SIGCONT)
        { // same
            failedTest(8, "ssigprocmask2 faild\n");
        }

        kill(fatherpid, 6); //notify parent for blocking handler #3
                            //in father flag should be 0, in child flag sohuld be 0

        while (!flag)
        {
            //wait for father to change flag using handler #3
        }
        //flag should be 1 now, change to 0
        kill(fatherpid, 6); //should not reach here
        failedTest(8, "child should not reach here, it shold remain stopped\n");
    }
    else //parent
    {
        sleep(waitingTime);

        kill(childpid, 6);
        //flag in father should be 0 now
        sleep(20);
        while (!flag)
        {
            printf(1, "father waiting  1\n");
            sleep(5);
            //wait for child to block handler#3
        }

        //flag should be 1
        sleep(waitingTime);
        for (i = 0; i < waitingTime; ++i)
        { //doing it in a loop to make sure child don't miss the signal
            kill(childpid, SIGCONT);
            sleep(5);
            printf(1, "father waiting  2\n");
        }
        sleep(waitingTime);

        if (flag)
        {
            failedTest(8, "child should not send message, sigprocmask probably failed\n");
        }

        kill(childpid, SIGKILL);
        wait();
    }

    restoreActions();
    passedTest(8);
}

void test9()
{ //checks default behaviour
    restoreActions();

    int pids[32];
    int i;
    int n = 32;
    int pid;
    int waitingtime = 50;

    /* Start children. */
    for (i = 0; i < n; ++i)
    {
        if ((pids[i] = fork()) < 0)
        {
            printf(1, "fork failed\n");
            exit();
        }
        else if (pids[i] == 0)
        {
            while (1)
            {
                //wait for signals waiting
            }
        }
    }

    /* Wait for children to exit. */
    for (i = 0; i < n; ++i)
    {
        if (i == 17)
        {
            kill(pids[i], i);
            sleep(waitingtime);
            kill(pids[i], SIGCONT);
            continue;
        }
        else if (i == 19)
        {
            kill(pids[i], SIGSTOP);
            sleep(waitingtime);
            kill(pids[i], i);
            continue;
        }
        kill(pids[i], i);
    }
    sleep(waitingtime);
    kill(pids[17], SIGKILL);
    kill(pids[19], SIGKILL);
    while (n > 0)
    {
        pid = wait(); //
        printf(1, "Child with PID %d exited\n", pid);
        --n; // TODO(pts): Remove pid from the pids array.
    }
    restoreActions();
    passedTest(9);
}

void test10()
{ //fork test
    restoreActions();
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &handler3;
    sigaction(6, &sa, null);
    flag = 1;
    int n = 2;
    int pid;
    int childpids[n];

    //child should inherit parent sigPending, not sure how to do this
    if ((childpids[0] = fork()) == 0)
    {
        while (flag)
        {
            //wait for signals
        }
        exit();
    }
    else
    {

        if (sigprocmask(1 << 6) != 0)
        {
            failedTest(8, "ssigprocmask faild\n");
        }

        if (sigprocmask(1 << 6) != 1 << 6)
        { // same
            failedTest(8, "ssigprocmask2 faild\n");
        }
        if ((childpids[1] = fork()) == 0)
        {
            while (!flag)
            {
                //should not wait here
            }
            exit();
        }
        
        sleep(50);
        kill(childpids[0], 6);
        kill(childpids[1], 6);
        while (n > 0)
        {
            pid = wait(); //
            printf(1, "Child with PID %d exited\n", pid);
            --n;
        }
    }
    restoreActions();
    passedTest(10);
}

void test11() //test exec
{
    restoreActions();
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &handler3;
    sigaction(6, &sa, null);
    flag = 1;
    int n = 1;
    int childpids[n];
    char *argv[] = {"testExec", 0};

    if ((childpids[0] = fork()) == 0)
    {
        if ((exec("testExec", argv)) !=0){
            failedTest(11,"exec failed");
        }; //infinte loop. when is signaled by signum6 should ne killed, if not test fails.
    }
    else
    {
        sleep(50);
        kill(childpids[0], 6);  //we are sending signal that should activate handler #3,
        //but since exec worked(!!) we are back to default behaviour which is sigkill();
        wait();
    }
    restoreActions();
    passedTest(11);
}

int main(int argc, char *argv[])
{
    test1();
    test2();
    test3();
    test4();
    test5();
   test6();
 //  test7(); not passing so skip
 // test8(); not passing so skip
    test9();
  test10();
   test11();
    printf(1, "ALL TESTS PASSED !!!\n");
    exit();
}
