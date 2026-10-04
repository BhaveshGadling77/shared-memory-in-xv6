#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"
#include "shm.h"
#include "proc.h"

int shmget(int key, int size, int shmflg){
  int i;
  int j;
  int npages;
  int shmid;

  if(size <= 0)
    return -1;

  npages = (size + PGSIZE - 1) / PGSIZE;

  if(npages > SHM_MAX_PAGES) {
    return -1;
  }

  acquire(&shm.lock);

  // Check whether key already exists

  for(i = 0; i < NSHM; i++){
    if(shm.seg[i].used && shm.seg[i].key == key){
      shmid = shm.seg[i].shmid;
      release(&shm.lock);
      return shmid;
    }
  }

  // Segment doesn't exist
 //  to check whetehr the flag ask to create that particular segment like 
  if(!(shmflg & IPC_CREAT)){
    release(&shm.lock);
    return -1;
  }

  // Find free slot

  for(i = 0; i < NSHM; i++){
    if(!shm.seg[i].used)
      break;
  }

  // here it means that no free slot is there like 
  if(i == NSHM){
    release(&shm.lock);
    return -1;
  }

  // Allocate pages

  for(j = 0; j < npages; j++){
    shm.seg[i].pages[j] = kalloc();

     // as during running all the global variables are initialized out to zero 
    if(shm.seg[i].pages[j] == 0){

      // deletion if allocation fails

      while(j > 0){
        j--;
        kfree(shm.seg[i].pages[j]);
      }

      release(&shm.lock);
      return -1;
    }
  }

  // Fill metadata

  shm.seg[i].used = 1;
  shm.seg[i].key = key;
  shm.seg[i].shmid = i + 1;
  shm.seg[i].size = size;
  shm.seg[i].npages = npages;
  shm.seg[i].nattach = 0;

  shmid = shm.seg[i].shmid;

  release(&shm.lock);

  return shmid;
}

void* shmat(int shmid, const void *shmaddr, int shmflg)
{
  
  int i;
  int j;
  uint va;
  int perm;

  struct proc *p = myproc();
  acquire(&shm.lock);

  // first find the shm segment using loop
  for (i = 0; i < NSHM; i++) {
    if(shm.seg[i].used &&
      shm.seg[i].shmid == shmid)
      break;
  }

  //not found segment error
  if (i == NSHM) {
    release(&shm.lock);
    return (void*)-1;
  }
  
  // lets find if the any fixed virtual address is given or not.
  if (shmaddr == 0) {
	va = PGROUNDUP(p->sz);
  } else {
	
  }
  
  release(&shm.lock);
  return (void *)va;
}
