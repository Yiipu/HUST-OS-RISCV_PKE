/*
 * Utility functions for process management. 
 *
 * Note: in Lab1, only one process (i.e., our user application) exists. Therefore, 
 * PKE OS at this stage will set "current" to the loaded user application, and also
 * switch to the old "current" process after trap handling.
 */

#include "riscv.h"
#include "strap.h"
#include "config.h"
#include "process.h"
#include "elf.h"
#include "string.h"
#include "vmm.h"
#include "pmm.h"
#include "memlayout.h"
#include "spike_interface/spike_utils.h"
#include "util/functions.h"

//Two functions defined in kernel/usertrap.S
extern char smode_trap_vector[];
extern void return_to_user(trapframe *, uint64 satp);

// current points to the currently running user-mode application.
process* current = NULL;

// points to the first free page in our simple heap. added @lab2_2
uint64 g_ufree_page = USER_FREE_ADDRESS_START;

//
// switch to a user-mode process
//
void switch_to(process* proc) {
  assert(proc);
  current = proc;

  // write the smode_trap_vector (64-bit func. address) defined in kernel/strap_vector.S
  // to the stvec privilege register, such that trap handler pointed by smode_trap_vector
  // will be triggered when an interrupt occurs in S mode.
  write_csr(stvec, (uint64)smode_trap_vector);

  // set up trapframe values (in process structure) that smode_trap_vector will need when
  // the process next re-enters the kernel.
  proc->trapframe->kernel_sp = proc->kstack;      // process's kernel stack
  proc->trapframe->kernel_satp = read_csr(satp);  // kernel page table
  proc->trapframe->kernel_trap = (uint64)smode_trap_handler;

  // SSTATUS_SPP and SSTATUS_SPIE are defined in kernel/riscv.h
  // set S Previous Privilege mode (the SSTATUS_SPP bit in sstatus register) to User mode.
  unsigned long x = read_csr(sstatus);
  x &= ~SSTATUS_SPP;  // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE;  // enable interrupts in user mode

  // write x back to 'sstatus' register to enable interrupts, and sret destination mode.
  write_csr(sstatus, x);

  // set S Exception Program Counter (sepc register) to the elf entry pc.
  write_csr(sepc, proc->trapframe->epc);

  // make user page table. macro MAKE_SATP is defined in kernel/riscv.h. added @lab2_1
  uint64 user_satp = MAKE_SATP(proc->pagetable);

  // return_to_user() is defined in kernel/strap_vector.S. switch to user mode with sret.
  // note, return_to_user takes two parameters @ and after lab2_1.
  return_to_user(proc->trapframe, user_satp);
}

//
// Insert a memory block into the free list following the Worst Fit strategy.
//
void insert_free_block(memblock_t** block_list, memblock_t* block){
  memblock_t *prev = *block_list;
  memblock_t *probe = *block_list;
  while(probe){
    if(probe->size <= block->size) break;
    prev = probe;
    probe = probe->next;
  }
  if(probe == *block_list){
    block->next = *block_list;
    *block_list = block;
  }else{
    prev->next = block;
    block->next = probe;
  }
}

void *memblock_alloc(uint64 size){
  // Round up to 8 bytes
  size = ROUNDUP(size, 8);
  if(size > PGSIZE) panic("memblock_alloc: size too large");

  // Find a free block
  memblock_t *b = current->free_block;

  // Note: using Worst Fit strategy, the head block is the largest one.

  if(b == NULL || b->size < size){
    // Allocate a new page
    void* pa = alloc_page();
    uint64 va = g_ufree_page;
    g_ufree_page += PGSIZE;
    user_vm_map((pagetable_t)current->pagetable, va, PGSIZE, (uint64)pa,
    prot_to_type(PROT_WRITE | PROT_READ, 1));

    // Turn the page into a memblock
    memblock_t *new = (memblock_t*)pa;
    new->start = va + sizeof(memblock_t);
    new->end = va + PGSIZE;
    new->size = PGSIZE-sizeof(memblock_t);
    new->next = NULL;

    // Merge with the last block in the previous page if possible
    memblock_t *prev = current->free_block;
    memblock_t *probe = current->free_block;
    while(probe){
      if(probe->end == va)
        break;
      prev = probe;
      probe = probe->next;
    }
    if(probe){
      probe->end = new->end;
      probe->size += PGSIZE;
      memset(new, 0, sizeof(memblock_t));
      b = probe;
      // To separate b from the free list.
      prev->next = b->next;
      b->next = NULL;
    }else{
      // b is allready separated.
      b = new;
    }
  }else{
    // To separate b from the free list.
    current->free_block = b->next;
  }

  // @assert b != NULL && b->size >= size

  // b now has 3 cases:
  // 1. b == current->free_block
  // 2. b is a new block, with size of PGSIZE - sizeof(memblock_t), CAN'T FIT SIZE > 2*PGSIZE
  // 3. b is the last block in the previous page, with extended size (size += PGSIZE)
  // In all cases, b is now seperated from the free list.

  // Split the block and update the free list if necessary
  if(b->size > size + sizeof(memblock_t)){
    memblock_t *leftover = (memblock_t *)user_va_to_pa((pagetable_t)current->pagetable,(void*)(b->start + size));
    leftover->start = b->start + size + sizeof(memblock_t);
    leftover->end = b->end;
    leftover->size = leftover->end - leftover->start;
    leftover->next = b->next;

    b->end = b->start + size;
    b->size = size;
    b->next = NULL;

    // leftover is a new free block, insert it into the free list.
    insert_free_block(&current->free_block, leftover);
  }

  // Add the block to the used list
  b->next = current->used_block;
  current->used_block = b;

  return (void*)b->start;
}

void memblock_free(uint64 ptr){
  // Find the block
  memblock_t *b = current->used_block;
  memblock_t *prev = current->used_block;
  while(b){
    if(b->start <= (uint64)ptr && b->end >= (uint64)ptr) break;
    prev = b;
    b = b->next;
  }
  if(b == NULL) panic("memblock_free: block not found");

  // Remove the block from the used list
  if(b == current->used_block)
    current->used_block = b->next;
  else
    prev->next = b->next;

  // Clear the block
  memset((void*)(b + sizeof(memblock_t)), 0, b->size);

  // Add the block to the free list
  insert_free_block(&current->free_block, b);
}