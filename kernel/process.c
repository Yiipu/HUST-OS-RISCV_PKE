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

#include "spike_interface/spike_utils.h"

//Two functions defined in kernel/usertrap.S
extern char smode_trap_vector[];
extern void return_to_user(trapframe*);

// current points to the currently running user-mode application.
process* current = NULL;

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
  proc->trapframe->kernel_sp = proc->kstack;  // process's kernel stack
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

  // return_to_user() is defined in kernel/strap_vector.S. switch to user mode with sret.
  return_to_user(proc->trapframe);
}

void print_error_line(uint64 mepc)
{
  // Locate the error line
  int i = 0;
  addr_line *pline = current->line;
  while(i < current->line_ind && pline->addr != mepc) ++i, ++pline;
  if(pline->addr != mepc) panic("Failed to find errorline!\n");

  addr_line *err_line = pline;
  code_file *err_file = current->file + err_line->file;

  char filepath[0x100];
  strcpy(filepath, (current->dir)[err_file->dir]);
  filepath[strlen(filepath)] = '/';
  strcpy(filepath + strlen(filepath), err_file->file);

  sprint("Runtime error at %s:%d\n", filepath, err_line->line);

  // Read the content of the line
  spike_file_t *file = spike_file_open(filepath, O_RDONLY, 0);
  if (IS_ERR_VALUE(file)) panic("Failed to open file!\n");

  char buf[0x1000];
  spike_file_pread(file, buf, sizeof(buf), 0);
  spike_file_close(file);

  uint64 line_start = 0;
  for(int i = 1; i < err_line->line; ++i)
  {
    while(buf[line_start] != '\n') ++line_start;
    ++line_start;
  }

  char* err_line_start = buf + line_start;

  char* p_err_line = buf + line_start;
  while (*p_err_line != '\n') ++p_err_line;
  *p_err_line = '\0';

  sprint("%s\n", err_line_start);
}