#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "pstat.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

struct pstat proc_stat;
extern int prqueue[4][NPROC + 1];

static void wakeup1(void *chan);

  void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// helper functions
inline int pow_of_two(int n) {
  switch(n) {
    case 0:
      return 1;
    case 1:
      return 2;
    case 2:
      return 4;
    case 3:
      return 8;
    default:
      cprintf("n is not 0, 1, 2 or 3.\n");
      return -1;
  }

}

// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
  static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  int slot_idx = -1;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    ++slot_idx;
    if(p->state == UNUSED)
      goto found;
  }
  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  proc_stat.inuse[slot_idx] = 1;
  proc_stat.pid[slot_idx] = p->pid;
  proc_stat.priority[slot_idx] = 0;
  cprintf("Before adding to the queue.\n");
  // Adds the new process to the priority queue.
  priqueue[0][priqueue[0][NPROC]] = slot_idx;
  ++priqueue[0][NPROC];

  release(&ptable.lock);

  // Allocate kernel stack if possible.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;
  cprintf("Before setting up new context.\n");

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

// TODO(byan23): Remove the initialization of queues.
// Set up first user process.
  void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  //memset(prqueue, 0, (NPROC + 1) * 4 * sizeof(int));

  p = allocproc();
  acquire(&ptable.lock);
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
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
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;
  release(&ptable.lock);
  cprintf("End of userinit.\n");
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
  int
growproc(int n)
{
  uint sz;

  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
  int
fork(void)
{
  int i, pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  // TODO(byan23): Delete the below print.
  cprintf("process: %d\n", np->pid);

  pid = np->pid;
  np->state = RUNNABLE;
  safestrcpy(np->name, proc->name, sizeof(proc->name));
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
  void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  iput(proc->cwd);
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
	wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
  int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
	continue;
      havekids = 1;
      if(p->state == ZOMBIE){
	// Found one.
	pid = p->pid;
	kfree(p->kstack);
	p->kstack = 0;
	freevm(p->pgdir);
	p->state = UNUSED;
	p->pid = 0;
	p->parent = 0;
	p->name[0] = 0;
	p->killed = 0;
	release(&ptable.lock);
	return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
  void
scheduler(void)
{
  //struct proc *p;

  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    // TODO(byan23): Implement scheduler.
    // Loops over the priority queues (from 0 to 3) looking for process to run.
    int i, j;
    for (i = 0; i < 4; ++i) {
      for (j = 0; j < priqueue[i][NPROC]; ++j) {
	if (ptable.proc[priqueue[i][j]].state != RUNNABLE) continue;
	//cprintf("Found a runnable proc.\n");
	proc = &ptable.proc[priqueue[i][j]];
	// Switch to chosen process.  It is the process's job
	// to release ptable.lock and then reacquire it
	// before jumping back to us.
	switchuvm(proc);
	proc->state = RUNNING;
	swtch(&cpu->scheduler, proc->context);
	switchkvm();

	// Process is done running for now.
	// It should have changed its p->state before coming back.
	proc = 0;
      }
    }

    // Updates pstat.
    /*for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
	continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      proc = p;
      switchuvm(p);
      p->state = RUNNING;
      swtch(&cpu->scheduler, proc->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }*/
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
  void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
  void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  proc->state = RUNNABLE;
  // Loop thru pstat.pid to find the slot number in ptable.
  int i;  // slot idx
  for (i = 0; i < NPROC; ++i) 
    if (proc_stat.pid[i] == proc->pid) break;
  //assert(i < NPROC);
  if (i == NPROC)
    cprintf("i == NPROC\n");
  int pri = proc_stat.priority[i];
  ++proc_stat.ticks[i][pri];
  // Updates priority queues here.
  int j;
  for (j = 0; j < NPROC; ++j)
    if (priqueue[pri][j] == i) break;
  //assert(j < NPROC);
  if (j == NPROC)
    cprintf("j == NPROC\n");
  int end = priqueue[pri][NPROC] - 1;
  if (proc_stat.ticks[i][pri] == pow_of_two(pri)) {
    for (; j < end - 1; ++j)
      priqueue[pri][j] = priqueue[pri][j+1];
    if (pri != 3) {
      priqueue[pri][end] = -1; // not necessary
      priqueue[pri][NPROC] = end;
      ++pri;
      priqueue[pri][priqueue[pri][NPROC]] = i;
      ++priqueue[pri][NPROC];
      proc_stat.priority[i] = pri;
    } else {
      // RR
      priqueue[pri][end] = i;
    }
  }
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
  void
forkret(void)
{
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
  void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  proc->chan = chan;
  proc->state = SLEEPING;
  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

// Wake up all processes sleeping on chan.
// The ptable lock must be held.
  static void
wakeup1(void *chan)
{
  struct proc *p;
  int slot_no = -1;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    ++slot_no;
    if(p->state == SLEEPING && p->chan == chan) {
      p->state = RUNNABLE;
      // move to the front of the queue.
      int i;  // queue idx
      int pri = proc_stat.priority[slot_no];
      for (i = 0; i < NPROC; ++i)
	if (priqueue[pri][i] == slot_no) break;
      for (; i > 0; --i)
	priqueue[pri][i] = priqueue[pri][i - 1];
      priqueue[pri][0] = slot_no;
    }
  }
}

// Wake up all processes sleeping on chan.
  void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
  int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
	p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
  void
procdump(void)
{
  static char *states[] = {
    [UNUSED]    "unused",
    [EMBRYO]    "embryo",
    [SLEEPING]  "sleep ",
    [RUNNABLE]  "runble",
    [RUNNING]   "run   ",
    [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
	cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

// TODO(byan23): Implements populating pstat struct.
int getpinfo(struct pstat *p) {
  memmove(p->inuse, proc_stat.inuse, NPROC);
  memmove(p->pid, proc_stat.pid, NPROC);
  memmove(p->priority, proc_stat.priority, NPROC);
  memmove(p->ticks, proc_stat.ticks, NPROC * 4);
  return proc->pid;
}
