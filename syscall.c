#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "syscall.h"

// User code makes a system call with INT T_SYSCALL.
// System call number in %eax.
// Arguments on the stack, from the user call to the C
// library system call function. The saved user %esp points
// to a saved program counter, and then the first argument.

static int
is_valid_user_range(uint addr, int size)
{
  struct proc *curproc = myproc();
  int k;

  if(size < 0 || addr + size < addr)
    return 0;

  // normal user address space
  if(addr < curproc->sz && addr + size <= curproc->sz)
    return 1;

  // shared memory address space
  if(addr >= SHMBASE && addr + size <= SHMLIMIT) {
    for(k = 0; k < MAX_SHM_ATTACH; k++) {
      if(curproc->shmattach[k].used) {
        uint seg_start = curproc->shmattach[k].va;
        uint seg_end = seg_start + curproc->shmattach[k].npages * PGSIZE;
        if(addr >= seg_start && addr + size <= seg_end)
          return 1;
      }
    }
  }

  return 0;
}

// Fetch the int at addr from the current process.
int
fetchint(uint addr, int *ip)
{
  if(!is_valid_user_range(addr, 4))
    return -1;
  *ip = *(int*)(addr);
  return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// Doesn't actually copy the string - just sets *pp to point at it.
// Returns length of string, not including nul.
int
fetchstr(uint addr, char **pp)
{
  char *s, *ep = 0;
  struct proc *curproc = myproc();
  int k;

  if(addr < curproc->sz) {
    ep = (char*)curproc->sz;
  } else if(addr >= SHMBASE && addr < SHMLIMIT) {
    for(k = 0; k < MAX_SHM_ATTACH; k++) {
      if(curproc->shmattach[k].used) {
        uint seg_start = curproc->shmattach[k].va;
        uint seg_end = seg_start + curproc->shmattach[k].npages * PGSIZE;
        if(addr >= seg_start && addr < seg_end) {
          ep = (char*)seg_end;
          break;
        }
      }
    }
  }

  if(ep == 0)
    return -1;

  *pp = (char*)addr;
  for(s = *pp; s < ep; s++){
    if(*s == 0)
      return s - *pp;
  }
  return -1;
}

// Fetch the nth 32-bit system call argument.
int
argint(int n, int *ip)
{
  return fetchint((myproc()->tf->esp) + 4 + 4*n, ip);
}

// Fetch the nth word-sized system call argument as a pointer
// to a block of memory of size bytes.  Check that the pointer
// lies within the process address space.
int
argptr(int n, char **pp, int size)
{
  int i;
 
  if(argint(n, &i) < 0)
    return -1;
  if(!is_valid_user_range((uint)i, size))
    return -1;
  *pp = (char*)i;
  return 0;
}

// Fetch the nth word-sized system call argument as a string pointer.
// Check that the pointer is valid and the string is nul-terminated.
// (There is no shared writable memory, so the string can't change
// between this check and being used by the kernel.)
int
argstr(int n, char **pp)
{
  int addr;
  if(argint(n, &addr) < 0)
    return -1;
  return fetchstr(addr, pp);
}

extern int sys_chdir(void);
extern int sys_close(void);
extern int sys_dup(void);
extern int sys_exec(void);
extern int sys_exit(void);
extern int sys_fork(void);
extern int sys_fstat(void);
extern int sys_getpid(void);
extern int sys_kill(void);
extern int sys_link(void);
extern int sys_mkdir(void);
extern int sys_mknod(void);
extern int sys_open(void);
extern int sys_pipe(void);
extern int sys_read(void);
extern int sys_sbrk(void);
extern int sys_sleep(void);
extern int sys_unlink(void);
extern int sys_wait(void);
extern int sys_write(void);
extern int sys_uptime(void);
extern int sys_shmget(void);
extern int sys_shmat(void);
extern int sys_shmdt(void);
extern int sys_shmctl(void);
extern int sys_mmap(void);
extern int sys_shm_open(void);
extern int sys_ftruncate(void);

static int (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
[SYS_pipe]    sys_pipe,
[SYS_read]    sys_read,
[SYS_kill]    sys_kill,
[SYS_exec]    sys_exec,
[SYS_fstat]   sys_fstat,
[SYS_chdir]   sys_chdir,
[SYS_dup]     sys_dup,
[SYS_getpid]  sys_getpid,
[SYS_sbrk]    sys_sbrk,
[SYS_sleep]   sys_sleep,
[SYS_uptime]  sys_uptime,
[SYS_open]    sys_open,
[SYS_write]   sys_write,
[SYS_mknod]   sys_mknod,
[SYS_unlink]  sys_unlink,
[SYS_link]    sys_link,
[SYS_mkdir]   sys_mkdir,
[SYS_close]   sys_close,
[SYS_shmget]  sys_shmget,
[SYS_shmat]   sys_shmat,
[SYS_shmdt]  sys_shmdt,
[SYS_shmctl] sys_shmctl,
[SYS_mmap]   sys_mmap,
[SYS_shm_open] sys_shm_open,
[SYS_ftruncate] sys_ftruncate,
};

void
syscall(void)
{
  int num;
  struct proc *curproc = myproc();

  num = curproc->tf->eax;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    curproc->tf->eax = syscalls[num]();
  } else {
    cprintf("%d %s: unknown sys call %d\n",
            curproc->pid, curproc->name, num);
    curproc->tf->eax = -1;
  }
}
