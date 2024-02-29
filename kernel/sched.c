/*
 * implementing the scheduler
 */

#include "sched.h"
#include "spike_interface/spike_utils.h"

process* ready_queue_head = NULL;
// added for lab3_challenge1
process* blocked_queue_head = NULL;

//
// insert a process, proc, into the END of ready queue.
//
void insert_to_ready_queue( process* proc ) {
  sprint( "going to insert process %d to ready queue.\n", proc->pid );
  // if the queue is empty in the beginning
  if( ready_queue_head == NULL ){
    proc->status = READY;
    proc->queue_next = NULL;
    ready_queue_head = proc;
    return;
  }

  // ready queue is not empty
  process *p;
  // browse the ready queue to see if proc is already in-queue
  for( p=ready_queue_head; p->queue_next!=NULL; p=p->queue_next )
    if( p == proc ) return;  //already in queue

  // p points to the last element of the ready queue
  if( p==proc ) return;
  p->queue_next = proc;
  proc->status = READY;
  proc->queue_next = NULL;

  return;
}

//
// insert a process into the END of blocked queue.
//
void insert_to_blocked_queue(process *proc) 
{
  if( blocked_queue_head == NULL ){
    proc->status = BLOCKED;
    proc->queue_next = NULL;
    blocked_queue_head = proc;
    return;
  }
  // blocked queue is not empty
  process *p;
  // traverse the blocked queue to see if proc is already in-queue
  for( p=blocked_queue_head; p->queue_next!=NULL; p=p->queue_next )
    if( p == proc ) return;  //already in queue

  // p points to the last element of the blocked queue
  if( p==proc ) return;

  p->queue_next = proc;
  proc->status = BLOCKED;
  proc->queue_next = NULL;
  return;
}

// added lab3_challenge1
void remove_from_blocked_queue(process *proc){
  // sprint( "going to remove process %d from blocked queue.\n", proc->pid );
  if (blocked_queue_head == NULL){
    return;
  }
  if(blocked_queue_head==proc){
    blocked_queue_head = blocked_queue_head->queue_next;
    return;
  }
  process *fast, *slow;
  fast = blocked_queue_head;
  slow = blocked_queue_head;
  while (fast != NULL){
    if (fast == proc){
      slow->queue_next = fast->queue_next;
      return;
    }
    slow = fast;
    fast = fast->queue_next;
  }
  return;
}

//
// debug function
//
void print_blocked_queue(){
  process *p;
  for (p = blocked_queue_head; p; p = p->queue_next){
    sprint("blocked queue: pid: %d, status: %d, waitpid: %d\n", p->pid, p->status, p->waitpid);
  }
}

//
// choose a proc from the ready queue, and put it to run.
// note: schedule() does not take care of previous current process. If the current
// process is still runnable, you should place it into the ready queue (by calling
// ready_queue_insert), and then call schedule().
//
extern process procs[NPROC];
void schedule() {
  // added lab3_challenge1
  if (blocked_queue_head){
    process *p;
    for (p = blocked_queue_head; p; p = p->queue_next){
      if (is_blocked(p) != -1){
        insert_to_ready_queue(p);
        remove_from_blocked_queue(p);
      }
    }
  }

  if ( !ready_queue_head ){
    // by default, if there are no ready process, and all processes are in the status of
    // FREE and ZOMBIE, we should shutdown the emulated RISC-V machine.
    int should_shutdown = 1;

    for( int i=0; i<NPROC; i++ )
      if( (procs[i].status != FREE) && (procs[i].status != ZOMBIE) ){
        should_shutdown = 0;
        sprint( "ready queue empty, but process %d is not in free/zombie state:%d\n", 
          i, procs[i].status );
      }

    if( should_shutdown ){
      sprint( "no more ready processes, system shutdown now.\n" );
      shutdown( 0 );
    }else{
      panic( "Not handled: we should let system wait for unfinished processes.\n" );
    }
  }

  current = ready_queue_head;
  assert( current->status == READY );
  ready_queue_head = ready_queue_head->queue_next;

  current->status = RUNNING;
  sprint( "going to schedule process %d to run.\n", current->pid );
  switch_to( current );
}

// added for lab3_challenge1
// 如果等待的进程没有结束，返回-1，否则返回等待的进程的pid
int is_blocked(process *proc){
  if(proc->waitpid==-1){
    for(int i = 0;i<NPROC;i++){
      if(procs[i].status==ZOMBIE && procs[i].parent==proc){
        return i;
      }
    }
  }else if(procs[proc->waitpid].status==ZOMBIE){
    return proc->waitpid;
  }
  
  return -1;
}