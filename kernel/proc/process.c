#include "process.h"
#include "../mm/vmm.h"
#include "../mm/heap.h"
#include "../lib/printf.h"
#include <stdint.h>
#include <stddef.h>

static process_t proc_table[MAX_PROCESSES];
static uint32_t next_pid = 0;

void proc_init(void)
{
  for (int i = 0; i < MAX_PROCESSES; i++)
  {
    proc_table[i].state = PROC_DEAD;
    proc_table[i].pid = 0;
  }

  // pid 0 - kernel process, uses the current (active) CR3
  process_t *kproc = &proc_table[0];
  kproc->pid = 0;
  kproc->state = PROC_RUNNING;
  kproc->exit_code = 0;

  uint64_t cr3;
  __asm__ volatile("movq %%cr3, %0" : "=r"(cr3));
  kproc->pagemap = cr3;

  // zero handle table
  for (int i = 0; i < MAX_HANDLES; i++)
    kproc->handles[i] = NULL;

  int n = 0;
  const char *kname = "kernel";
  while (kname[n] && n < PROCESS_NAME_LEN - 1)
  {
    kproc->name[n] = kname[n]; n++;
  }
  kproc->name[n] = '\0';

  next_pid = 1;
  kprintf("proc: kernel process created (pid 0)\n");
}

process_t *proc_create(const char *name) 
{
  // find a free slot
  int slot = -1;
  for (int i = 1; i < MAX_PROCESSES; i++)
  {
    if (proc_table[i].state == PROC_DEAD) { slot = i; break; }
  }
  if (slot < 0)
  {
    kprintf("proc: process table full\n");
    return NULL;
  }

  process_t *p = &proc_table[slot];
  p->pid = next_pid++;
  p->state = PROC_RUNNING;
  p->exit_code = 0;
  p->pagemap = vmm_new_pagemap();

  for (int i = 0; i < MAX_HANDLES; i++)
    p->handles[i] = NULL;

  int n = 0;
  while (name[n] && n < PROCESS_NAME_LEN - 1)
  {
    p->name[n] = name[n]; n++;
  }
  p->name[n] = '\0';

  kprintf("proc: create process %u '%s'\n", p->pid, p->name);
  return p;
}

void proc_exit(process_t *proc, int32_t code)
{
  if (!proc) return;
  proc->exit_code = code;
  proc->state = PROC_ZOMBIE;
  kprintf("proc: process %u '%s' exited with code %d\n", proc->pid, proc->name, code);
  // TODO: reap threads, free pagemap, close handles
}

process_t *proc_get(uint32_t pid)
{
  for (int i = 0; i < MAX_PROCESSES; i++)
    if (proc_table[i].state != PROC_DEAD && proc_table[i].pid == pid)
      return &proc_table[i];
  return NULL;
}

process_t *proc_kernel(void)
{
  return &proc_table[0];
}
