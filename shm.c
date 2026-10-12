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

  // validate size bounds and prevent integer overflow
  if(size < 0 || size > SHM_MAX_PAGES * PGSIZE)
    return -1;

  acquire(&shm.lock);

  // Check whether key already exists (Bug 6: skip if key == IPC_PRIVATE)
  if(key != IPC_PRIVATE) {
    for(i = 0; i < NSHM; i++){
      if(shm.seg[i].used && shm.seg[i].key == key){
        // IPC_CREAT | IPC_EXCL fails if key exists
        if((shmflg & IPC_CREAT) && (shmflg & IPC_EXCL)) {
          release(&shm.lock);
          return -1;
        }
        // requested size exceeds existing segment size
        if(size > shm.seg[i].size) {
          release(&shm.lock);
          return -1;
        }
        // size == 0 should succeeds for existing segment
        shmid = shm.seg[i].shmid;
        release(&shm.lock);
        return shmid;
      }
    }
  }

  // segment doesn't exist
  // cannot create a new segment with size 0
  if(size == 0) {
    release(&shm.lock);
    return -1;
  }

  // key doesn't exist, must have IPC_CREAT
  if(key != IPC_PRIVATE && !(shmflg & IPC_CREAT)){
    release(&shm.lock);
    return -1;
  }

  npages = (size + PGSIZE - 1) / PGSIZE;
  if(npages <= 0 || npages > SHM_MAX_PAGES) {
    release(&shm.lock);
    return -1;
  }

  // Find free slot
  for(i = 0; i < NSHM; i++){
    if(!shm.seg[i].used)
      break;
  }

  // No free slot available
  if(i == NSHM){
    release(&shm.lock);
    return -1;
  }

  // Allocate pages
  for(j = 0; j < npages; j++){
    shm.seg[i].pages[j] = kalloc();

    if(shm.seg[i].pages[j] == 0){
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
  shm.seg[i].creator_pid = myproc() ? myproc()->pid : 0;

  shmid = shm.seg[i].shmid;

  release(&shm.lock);

  return shmid;
}

void* shmat(int shmid, const void *shmaddr, int shmflg)
{
  int i;
  int j;
  int k;
  uint va = 0;
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

  // not found segment error
  if (i == NSHM) {
    cprintf("shmat: segment not found, shmid=%d\n", shmid);
    release(&shm.lock);
    return (void*)-1;
  }
  
  // Validate flags
  if(shmflg & ~(SHM_RDONLY | SHM_RND | SHM_EXEC | SHM_REMAP)) {
    release(&shm.lock);
    return (void*)-1;
  }
  
  // SHM_REMAP requires non-null shmaddr
  if((shmflg & SHM_REMAP) && shmaddr == 0) {
    release(&shm.lock);
    return (void*)-1;
  }

  // automatic address selection: find free range in [SHMBASE, SHMLIMIT)
  // this the logical schema we choosed
  if(shmaddr == 0) {
    uint try_va;
    int found = 0;
    uint need_bytes = shm.seg[i].npages * PGSIZE;

    for(try_va = SHMBASE; try_va + need_bytes <= SHMLIMIT; try_va += PGSIZE) {
      int conflict = 0;
      for(j = 0; j < shm.seg[i].npages; j++) {
        pte_t *pte = walkpgdir(p->pgdir, (void*)(try_va + j * PGSIZE), 0);
        if(pte && (*pte & PTE_P)) {
          conflict = 1;
          break;
        }
      }
      if(!conflict) {
        va = try_va;
        found = 1;
        break;
      }
    }

    if(!found) {
      release(&shm.lock);
      return (void*)-1;
    }
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

  // Bug 8: If SHM_REMAP, clear existing mappings and clean overlapping attachments
  if(shmflg & SHM_REMAP) {
    for(j = 0; j < shm.seg[i].npages; j++) {
      pte_t *pte = walkpgdir(p->pgdir, (void*)(va + j * PGSIZE), 0);
      if(pte && (*pte & PTE_P)) {
        *pte = 0;
      }
    }

    for(k = 0; k < MAX_SHM_ATTACH; k++) {
      if(p->shmattach[k].used) {
        uint a_start = p->shmattach[k].va;
        uint a_end = a_start + p->shmattach[k].npages * PGSIZE;
        if(a_start < end && a_end > va) {
          int old_s;
          for(old_s = 0; old_s < NSHM; old_s++) {
            if(shm.seg[old_s].used && shm.seg[old_s].shmid == p->shmattach[k].shmid) {
              if(shm.seg[old_s].nattach > 0)
                shm.seg[old_s].nattach--;
              break;
            }
          }
          p->shmattach[k].used = 0;
          p->shmattach[k].shmid = 0;
          p->shmattach[k].va = 0;
          p->shmattach[k].npages = 0;
          p->shmattach[k].shmflg = 0;
        }
      }
    }
    if(p == myproc())
      lcr3(V2P(p->pgdir));
  } else {
    // If not remapping, ensure target virtual addresses are unused
    for(j = 0; j < shm.seg[i].npages; j++) {
      pte_t *pte = walkpgdir(p->pgdir, (void*)(va + j * PGSIZE), 0);
      if(pte && (*pte & PTE_P)) {
        release(&shm.lock);
        return (void*)-1;
      }
    }
  }

  // find a free attachment slot BEFORE mapping pages
  for(k = 0; k < MAX_SHM_ATTACH; k++) {
    if(!p->shmattach[k].used)
      break;
  }

  if(k == MAX_SHM_ATTACH) {
    cprintf("shmat: no free attachment slot\n");
    release(&shm.lock);
    return (void*)-1;
  }

  /* permission assignment */
  perm = PTE_U;

  if(!(shmflg & SHM_RDONLY)) {
    perm |= PTE_W;
  }

  // map every page of the shared memory segment.
  for(j = 0; j < shm.seg[i].npages; j++) {
    uint pa = V2P(shm.seg[i].pages[j]);
    if(mappages(p->pgdir,
            (void*)(va + j * PGSIZE),
            PGSIZE,
            pa,
            perm) < 0) {

      // remove pages mapped successfully before this failure.
      while(j > 0) {
        pte_t *pte;
        j--;

        pte = walkpgdir(p->pgdir,
              (void*)(va + j * PGSIZE),
              0);

        if(pte)
          *pte = 0;
      }

      if(p == myproc())
        lcr3(V2P(p->pgdir));

      release(&shm.lock);
      return (void*)-1;
    } 
  }

  p->shmattach[k].used = 1;
  p->shmattach[k].shmid = shmid;
  p->shmattach[k].va = va;
  p->shmattach[k].npages = shm.seg[i].npages;
  p->shmattach[k].shmflg = shmflg;

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

static void
free_shm_segment(struct shmseg *s)
{
  int j;
  for(j = 0; j < s->npages; j++) {
    if(s->pages[j]) {
      kfree(s->pages[j]);
      s->pages[j] = 0;
    }
  }
  s->used = 0;
  s->key = 0;
  s->shmid = 0;
  s->size = 0;
  s->npages = 0;
  s->nattach = 0;
  s->creator_pid = 0;
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

      // remove this process va -> pa mappings
      for(j = 0; j < s->npages; j++) {
        pte = walkpgdir(p->pgdir,
                        (void*)(va + j * PGSIZE),
                        0);

        if(pte)
          *pte = 0;
      }

      // update the attachment count.
      if(s->nattach > 0)
        s->nattach--;

      // Bug 7: Free segment and pages if no processes are attached anymore
      if(s->nattach == 0) {
        free_shm_segment(s);
      }
    }

    // Mark this attachment slot as unused.
    p->shmattach[k].used = 0;
    p->shmattach[k].shmid = 0;
    p->shmattach[k].va = 0;
    p->shmattach[k].npages = 0;
    p->shmattach[k].shmflg = 0;
  }

  // Bug 7: Also free any segment created by this process that was never attached
  for(i = 0; i < NSHM; i++) {
    if(shm.seg[i].used && shm.seg[i].creator_pid == p->pid && shm.seg[i].nattach == 0) {
      free_shm_segment(&shm.seg[i]);
    }
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