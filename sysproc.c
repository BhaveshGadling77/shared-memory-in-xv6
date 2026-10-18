#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "shm.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_shmdt(void)
{
  int shmaddr;

  if(argint(0, &shmaddr) < 0)
    return -1;

  return shmdt((const void*)shmaddr);
}

int
sys_shmctl(void)
{
  int shmid, cmd, bufaddr;
  char *buf;
  int size;

  if(argint(0, &shmid) < 0)
    return -1;

  if(argint(1, &cmd) < 0)
    return -1;

  if(argint(2, &bufaddr) < 0)
    return -1;
  buf = (char*)bufaddr;

  /*
   * Determine the buffer size required by the command.
   */
  if(cmd == IPC_STAT || cmd == IPC_SET ||
     cmd == SHM_STAT || cmd == SHM_STAT_ANY)
    size = sizeof(struct shmctl_ds);
  else if(cmd == IPC_INFO || cmd == SHM_INFO)
    size = sizeof(struct shmctl_info);
  else if(cmd == IPC_RMID)
    size = 0;
  else
    return -1;

  /*
   * Validate the user buffer before accessing it.
   */
  if(cmd != IPC_RMID) {
    if(buf == 0)
      return -1;

    if(argptr(2, &buf, size) < 0)
      return -1;
  }

  return shmctl(shmid, cmd, buf);
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int sys_shmget(void) {
  int key;
  int size;
  int shmflg;

  if(argint(0, &key) < 0)
    return -1;

  if(argint(1, &size) < 0)
    return -1;

  if(argint(2, &shmflg) < 0)
    return -1;

  return shmget(key, size, shmflg);
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int
sys_shmat(void)
{
  int shmid;
  int shmflg;
  int shmaddr;

  if(argint(0, &shmid) < 0)
    return -1;

  if(argint(1, &shmaddr) < 0)
    return -1;

  if(argint(2, &shmflg) < 0)
    return -1;

  return (int)shmat(shmid, (const void*)shmaddr, shmflg);
}
