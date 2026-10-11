#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
// #include "spinlock.h"
#include "shm.h"
#include "proc.h"
#include "x86.h"

struct shmstate shm;

void shminit(void)
{
  initlock(&shm.lock, "shm");
}

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
        shm.seg[i].pages[j] = 0;
      }

      release(&shm.lock);
      return -1;
    }

    memset(shm.seg[i].pages[j], 0, PGSIZE);

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
  int k;
  uint va;
  uint end;
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
    cprintf("shmat: segment not found, shmid=%d\n", shmid);
    release(&shm.lock);
    return (void*)-1;
  }
  
  // Validate flags first.
  if(shmflg & ~(SHM_RDONLY | SHM_RND | SHM_EXEC | SHM_REMAP)) {
    release(&shm.lock);
    return (void*)-1;
  }
  
  // SHM_REMAP is not implemented yet.
  if(shmflg & SHM_REMAP) {
    release(&shm.lock);
    return (void*)-1;
  }

  // lets find if the any fixed virtual address is given or not.
  if(shmaddr == 0) {
    va = PGROUNDUP(p->sz);

    if(va < SHMBASE)
      va = SHMBASE;
  } else {
    va = (uint)shmaddr;

  if(va % PGSIZE) {
    if(shmflg & SHM_RND)
      va = PGROUNDDOWN(va);
    else {
      release(&shm.lock);
      return (void*)-1;
    }
  }
}

end = va + shm.seg[i].npages * PGSIZE;

if(va < SHMBASE || end < va || end > SHMLIMIT) {
  release(&shm.lock);
  return (void*)-1;
}

// Find a free attachment slot BEFORE mapping pages
for(k = 0; k < MAX_SHM_ATTACH; k++) {
  if(!p->shmattach[k].used)
    break;
}

if(k == MAX_SHM_ATTACH) {
  cprintf("shmat: no free attachment slot\n");
  release(&shm.lock);
  return (void*)-1;
}

// Ensure the target virtual addresses are unused.
for(j = 0; j < shm.seg[i].npages; j++) {
  pte_t *pte;

  pte = walkpgdir(p->pgdir,
                  (void*)(va + j * PGSIZE),
                  0);

  if(pte && (*pte & PTE_P)) {
    release(&shm.lock);
    return (void*)-1;
  }
}


/* permission assignment */
perm = PTE_U;

if(!(shmflg & SHM_RDONLY)) {
  perm |= PTE_W;
}

  // // shared memory should be user accessible and writable.
  // perm = PTE_W | PTE_U;

  // map every page of the shared memory segment.
  for(j = 0; j < shm.seg[i].npages; j++) {

    uint pa = V2P(shm.seg[i].pages[j]);
    //mappages internally check if the page is allocated to some other processes or not.
    if(mappages(p->pgdir,
            (void*)(va + j * PGSIZE),
            PGSIZE,
            pa,
            perm) < 0) {

  // Remove pages mapped successfully before this failure.
            while(j > 0) {
              pte_t *pte;
              j--;

              pte = walkpgdir(p->pgdir,
                    (void*)(va + j * PGSIZE),
                    0);

              if(pte)
                *pte = 0;
            }

      // Flush the TLB for the current process.
            if(p == myproc())
              lcr3(V2P(p->pgdir));

            release(&shm.lock);
            return (void*)-1;
    } 
  }

  p->shmattach[k].used = 1;
  p->shmattach[k].shmid = shmid;
  p->shmattach[k].va = va;

  shm.seg[i].nattach++;

  if(p == myproc())
    lcr3(V2P(p->pgdir));

  release(&shm.lock);

  return (void*)va;
}

int
is_shared_page(uint pa)
{
  int i;
  int j;

  acquire(&shm.lock);

  for(i = 0; i < NSHM; i++) {
    if(!shm.seg[i].used)
      continue;

    for(j = 0; j < shm.seg[i].npages; j++) {
      if(V2P(shm.seg[i].pages[j]) == pa) {
        release(&shm.lock);
        return 1;
      }
    }
  }

  release(&shm.lock);
  return 0;
}

int
get_shm_segment(uint pa)
{
  int i;
  int j;

  acquire(&shm.lock);

  for(i = 0; i < NSHM; i++) {
    if(!shm.seg[i].used)
      continue;

    for(j = 0; j < shm.seg[i].npages; j++) {
      if(V2P(shm.seg[i].pages[j]) == pa) {
        release(&shm.lock);
        return i;
      }
    }
  }

  release(&shm.lock);
  return -1;
}

void
shmexit(struct proc *p)
{
  int i;
  int j;
  int k;
  uint va;
  struct shmseg *s;
  pte_t *pte;

  acquire(&shm.lock);

  for(k = 0; k < MAX_SHM_ATTACH; k++) {
    if(!p->shmattach[k].used)
      continue;

    // Find the segment associated with this attachment.
    for(i = 0; i < NSHM; i++) {
      if(shm.seg[i].used &&
         shm.seg[i].shmid == p->shmattach[k].shmid)
        break;
    }

    if(i < NSHM) {
      s = &shm.seg[i];
      va = p->shmattach[k].va;

      // Remove this process's mappings, not the physical pages.
      for(j = 0; j < s->npages; j++) {
        pte = walkpgdir(p->pgdir,
                        (void*)(va + j * PGSIZE),
                        0);

        if(pte)
          *pte = 0;
      }

      // Update the attachment count.
      if(s->nattach > 0)
        s->nattach--;
    }

    // Mark this attachment slot as unused.
    p->shmattach[k].used = 0;
    p->shmattach[k].shmid = 0;
    p->shmattach[k].va = 0;
  }

  release(&shm.lock);

  // Flush the TLB if this is the currently running process.
  if(p == myproc() && p->pgdir != 0)
    lcr3(V2P(p->pgdir));
}

int
shmfork(struct proc *parent, struct proc *child)
{
  int i, j, k;
  uint va;
  pte_t *pte;
  pte_t *cpte;
  struct shmseg *s;

  acquire(&shm.lock);

  for(k = 0; k < MAX_SHM_ATTACH; k++) {
    if(!parent->shmattach[k].used)
      continue;

    // Find a free attachment slot in the child.
    for(i = 0; i < MAX_SHM_ATTACH; i++) {
      if(!child->shmattach[i].used)
        break;
    }

    if(i == MAX_SHM_ATTACH){
      cprintf("shmfork: no free child attachment slot\n");
      goto bad;
    }
    // Find the shared-memory segment.
    for(j = 0; j < NSHM; j++) {
      if(shm.seg[j].used &&
         shm.seg[j].shmid == parent->shmattach[k].shmid)
        break;
    }

    if(j == NSHM){
      cprintf("shmfork: segment not found\n");
      goto bad;
    }
    s = &shm.seg[j];
    va = parent->shmattach[k].va;

    // Verify that the child's corresponding pages exist
    // and are not already mapped.
    for(j = 0; j < s->npages; j++) {
      pte = walkpgdir(parent->pgdir,
                      (void*)(va + j * PGSIZE), 0);
      cpte = walkpgdir(child->pgdir,
                       (void*)(va + j * PGSIZE), 1);

      // if(pte == 0 || !(*pte & PTE_P) ||
      //    cpte == 0 || (*cpte & PTE_P)){
      //    cprintf("shmfork: PTE validation failed at va 0x%x\n", va + j * PGSIZE);
      //    goto bad;
      // }

      if(pte == 0 || !(*pte & PTE_P)) {
          cprintf("shmfork: parent PTE missing at 0x%x\n",
          va + j * PGSIZE);
          goto bad;
      }

      if(cpte == 0) {
        cprintf("shmfork: child page-table allocation failed\n");
        goto bad;
      }

      if(*cpte & PTE_P) {
        cprintf("shmfork: child PTE already present at 0x%x\n",
        va + j * PGSIZE);
        goto bad;
      }
    }

    // Map the same physical shared pages into the child.
    for(j = 0; j < s->npages; j++) {
      pte = walkpgdir(parent->pgdir,
                      (void*)(va + j * PGSIZE), 0);
      cpte = walkpgdir(child->pgdir,
                       (void*)(va + j * PGSIZE), 0);

      *cpte = PTE_ADDR(*pte) | (PTE_FLAGS(*pte));
    }

    // Record the inherited attachment.
    child->shmattach[i].used = 1;
    child->shmattach[i].shmid = parent->shmattach[k].shmid;
    child->shmattach[i].va = va;
    child->shmattach[i].npages = parent->shmattach[k].npages;
    child->shmattach[i].shmflg = parent->shmattach[k].shmflg;

    s->nattach++;
  }

  release(&shm.lock);
  return 0;

bad:
  // Roll back all mappings and attachment records created for
  // the child during this operation.
  for(k = 0; k < MAX_SHM_ATTACH; k++) {
    if(!child->shmattach[k].used)
      continue;

    for(i = 0; i < NSHM; i++) {
      if(shm.seg[i].used &&
         shm.seg[i].shmid == child->shmattach[k].shmid)
        break;
    }

    if(i < NSHM) {
      s = &shm.seg[i];
      va = child->shmattach[k].va;

      for(j = 0; j < s->npages; j++) {
        pte = walkpgdir(child->pgdir,
                        (void*)(va + j * PGSIZE), 0);
        if(pte)
          *pte = 0;
      }

      if(s->nattach > 0)
        s->nattach--;
    }

    child->shmattach[k].used = 0;
    child->shmattach[k].shmid = 0;
    child->shmattach[k].va = 0;
    child->shmattach[k].npages = 0;
    child->shmattach[k].shmflg = 0;
  }

  release(&shm.lock);
  return -1;
}