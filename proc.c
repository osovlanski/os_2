#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
//#include "string.c"

struct
{
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);
extern void afterhandling(void);

static void wakeup1(void *chan);


void pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int cpuid()
{
  return mycpu() - cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu *
mycpu(void)
{
  int apicid, i;

  if (readeflags() & FL_IF)
    panic("mycpu called with interrupts enabled\n");

  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i)
  {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc *
myproc(void)
{
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

int allocpid(void)
{
  //old:
  // int pid;
  // acquire(&ptable.lock);
  // pid = nextpid++;
  // release(&ptable.lock);
  // return pid;

  //new:
  int pid;
  do {
    pid = nextpid;
  } while (!cas(&nextpid, pid, pid + 1));
  return pid;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc *
allocproc(void)
{
  struct proc *p;
  char *sp;

  //(old part 1)
  // acquire(&ptable.lock);

  // for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  //   if (p->state == UNUSED)
  //     goto found;

  // release(&ptable.lock);

  // return 0;

  //new
  pushcli();
  do {
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
      if(p->state == UNUSED)
        break;
    if (p == &ptable.proc[NPROC]) {
      popcli();
      return 0; // ptable is full
    }
  } while (!cas(&p->state, UNUSED, EMBRYO));
  popcli();

  //old (part 2):
//found:
  // p->state = EMBRYO;
  // release(&ptable.lock);

  p->pid = allocpid();

  // Allocate kernel stack.
  if ((p->kstack = kalloc()) == 0)
  {
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe *)sp;

  // Set up new context to start executing at forkret,

  // which returns to trapret.
  sp -= 4;
  *(uint *)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context *)sp;
  memset(p->context, 0, sizeof *p->context);

  //push handlers to start of stack (for avoiding overriding data)
  char *sp1;  //stack pointer
  sp1 = p->kstack + (MAXSIG * sizeof(struct sigaction)) + sizeof *p->userTfBackup;
  sp1 -= sizeof *p->userTfBackup;
  p->userTfBackup = (struct trapframe *)sp1;

  for (int i = 0; i < MAXSIG; i++)
  {
    sp1 -= sizeof(struct sigaction);
    p->sighandler[i] = (struct sigaction *)sp1;
    p->sighandler[i]->sa_handler = SIG_DFL;
    p->sighandler[i]->sigmask = 0;
  }

  sigemptyset(&p->sigMask);
  sigemptyset(&p->sigPending);

  p->context->eip = (uint)forkret;

  p->ignoreSignal = 0;
  //p->frozen = 0;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();

  initproc = p;
  if ((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0; // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if (n > 0)
  {
    if ((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  else if (n < 0)
  {
    if ((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if ((np = allocproc()) == 0)
  {
    return -1;
  }

  // Copy process state from proc.
  if ((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0)
  {
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for (i = 0; i < NOFILE; i++)
    if (curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));
  //memcpy((void*)np->sighandler, (void *)curproc->sighandler, 32 * sizeof(void *));
  for (int i =0;i<MAXSIG;i++){
    *np->sighandler[i]=*curproc->sighandler[i];
    //*np->sighandler[i]->sa_handler = *curproc->sighandler[i]->sa_handler;
    //*np->sighandler[i]->sigmask = *curproc->sighandler[i]->sigmask;
  }

  np->sigMask = curproc->sigMask;

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if (curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for (fd = 0; fd < NOFILE; fd++)
  {
    if (curproc->ofile[fd])
    {
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->parent == curproc)
    {
      p->parent = initproc;
      if (p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for (;;)
  {
    // Scan through table looking for exited children.
    havekids = 0;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->parent != curproc)
        continue;
      havekids = 1;
      if (p->state == ZOMBIE)
      {
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || curproc->killed)
    {
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock); //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;

  for (;;)
  {
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      
      //if kill request and proc is sleep - than wakeup
      if (p->state == SLEEPING && p->killRequest > 0){
        //cprintf("aaaa %d \n",p->frozen > 0 && p->contRequest == 0 && p->killRequest == 0);
        p->state = RUNNABLE;
      }
        
      if(p->frozen > 0 && p->contRequest == 0 && p->killRequest == 0)
        continue;    
  
      if (p->state != RUNNABLE) 
        continue;

       
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;
      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void)
{
  int intena;
  struct proc *p = myproc();

  if (!holding(&ptable.lock))
    panic("sched ptable.lock");
  if (mycpu()->ncli != 1)
    panic("sched locks");
  if (p->state == RUNNING)
    panic("sched running");
  if (readeflags() & FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void)
{
  acquire(&ptable.lock); //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first)
  {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if (p == 0)
    panic("sleep");

  if (lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if (lk != &ptable.lock)
  {                        //DOC: sleeplock0
    acquire(&ptable.lock); //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if (lk != &ptable.lock)
  { //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}


void sigign(int i)
{
  cprintf("ignore: %d\n",i);
}


int registerSig(int signum, struct sigaction *act, struct sigaction *oldact)
{
  if (signum < 0 || signum > 31)
  {
    return -1;
  }

  struct proc *p = myproc();
  if (oldact != null)
  {
    *oldact = *p->sighandler[signum];
  }
  if (act != null){
    if (signum == SIGSTOP || signum == SIGKILL)
      return -1;
    *p->sighandler[signum] = *act;
  }

  return 0;
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int kill(int pid, int signum)
{
  struct proc *p;

  if (signum < 0 || signum > 31)
  {
    return -1;
  }

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->pid == pid)
    {
      //if (signum == SIGKILL || signum == SIGSTOP || !sigismember(&p->sigMask, signum))
      { //cannot ignore SIGSTOP and SIGKILL
        sigup(&p->sigPending, signum);

        //scenarios: 
        //1. cast custome action is also sigcont
        //2. kill after stop (without cont)
        //3. kill after sleep 
        uint sa = (uint)p->sighandler[signum]->sa_handler;
        if ((signum == SIGCONT && sa == SIG_DFL)  || sa == SIGCONT  /*signum == SIGCONT || sa == SIGCONT*/){
          p->contRequest = 1;
        }

        if (signum == SIGKILL || (sa == SIG_DFL && signum != SIGSTOP && signum != SIGCONT) /*signum == SIGCONT || sa == SIGCONT*/){
          p->killRequest = 1;
        }
      }
      

      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void procdump(void)
{
  static char *states[] = {
      [UNUSED] "unused",
      [EMBRYO] "embryo",
      [SLEEPING] "sleep ",
      [RUNNABLE] "runble",
      [RUNNING] "run   ",
      [ZOMBIE] "zombie"};
  //[FROZEN] "frozen"};
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if (p->state == SLEEPING)
    {
      getcallerpcs((uint *)p->context->ebp + 2, pc);
      for (i = 0; i < 10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

int sigret(void)
{
  struct proc *p = myproc();
  *p->tf = *p->userTfBackup;
  p->sigMask = p->backupMask;
  p->ignoreSignal = 0;

  return 0;
}

void handleKernelSignal(int signum)
{
  struct proc *p = myproc();
  p->backupMask = p->sigMask;
  p->sigMask = p->sighandler[signum]->sigmask;
  p->sighandler[signum]->sa_handler(p->pid);
  p->sigMask = p->backupMask;
}
//
int myPop()
{
  int signum = 0;
  acquire(&ptable.lock);
  do
  {
    if (sigismember(&myproc()->sigPending, signum))
    {
      sigdown(&myproc()->sigPending, signum);
      break;
    }
    signum += 1;

  } while (signum < MAXSIG);

  release(&ptable.lock);
  return signum;
}


void sigkill(struct proc * p){
  p->killed = 1;
  p->killRequest = 0;
  if (p->state == SLEEPING)
    p->state = RUNNABLE;
  p->sigMask = p->backupMask;
}

void sigstop(struct proc * p){
  p->frozen = 1;
  p->sigMask = p->backupMask;
  yield();
}

void sigcont(struct proc * p){
  p->frozen = 0;
  p->contRequest = 0;
  p->sigMask = p->backupMask;
}

void handlingSignals(struct trapframe *tf)
{
  struct proc *p = myproc();
  if (p == null) return;
  if (p->ignoreSignal) return;
  if (p->killRequest == 0 && (tf->cs & 3) != DPL_USER) return; // CPU in user mode
  if (p->sigPending == 0) return;
  
  int signum = myPop();
  uint sa = (uint)p->sighandler[signum]->sa_handler;
  //cprintf("sa:::: %d   signum:::::%d\n",sa,signum);
  
  if (sa == SIG_IGN) return;
  //kernel
  p->backupMask = p->sigMask;
  p->sigMask = p->sighandler[signum]->sigmask;
  if ((sa == SIG_DFL && signum == SIGKILL) || sa == SIGKILL){
    sigkill(p);
    return;
  }
  if ((sa == SIG_DFL && signum == SIGSTOP) || sa == SIGSTOP){
    sigstop(p);
    return;
  }
  
  if (sigismember(&p->sigMask, signum)){
    cprintf("signum %d is blocked\n",signum);
    sigup(&p->sigPending, signum);

    //Evil Sceneario !!!!
    if((sa == SIG_DFL && signum == SIGCONT) || sa == SIGCONT) {
      do{
        yield();
      }while(sigismember(&p->sigMask, signum) && p->killRequest == 0);

      sigcont(p);
    }

    return;
  }
 

  if ((sa == SIG_DFL && signum == SIGCONT) || sa == SIGCONT){
    sigcont(p);
    return;
  }
  //else :if still sig_default -> the behaviour is like sigkill
  if(sa == SIG_DFL){
     sigkill(p);
    return;
  } 
  p->sigMask = p->backupMask;
    
  //user
  p->ignoreSignal = 1;
  *p->userTfBackup = *p->tf;
  p->backupMask = p->sigMask;
  p->sigMask = p->sighandler[signum]->sigmask;
  p->tf->esp -= (uint)invoke_sigret_end - (uint)invoke_sigret_start;
  memmove((void *)p->tf->esp, invoke_sigret_start, (uint)invoke_sigret_end - (uint)invoke_sigret_start);
  *((int *)(p->tf->esp - 4)) = signum;
  *((int *)(p->tf->esp - 8)) = p->tf->esp; // sigret system call code address
  p->tf->esp -= 8;
  p->tf->eip = (uint)p->sighandler[signum]->sa_handler;
}
