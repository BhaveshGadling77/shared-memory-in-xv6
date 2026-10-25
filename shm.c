#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"
#include "shm.h"
#include "proc.h"
#include "x86.h"

struct shmtable {
  struct spinlock lock;
  struct shmseg seg[NSHM];
};

struct shmtable shm;
static void free_shm_segment(struct shmseg *s);
static int shmdt_internal(struct proc *p, uint va);

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
      if(shm.seg[i].used && !shm.seg[i].marked_for_removal && shm.seg[i].key == key){
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
  safestrcpy(shm.seg[i].creator_name,
             myproc() ? myproc()->name : "",
             sizeof(shm.seg[i].creator_name));
  shm.seg[i].mode = shmflg & 0777;
  shm.seg[i].marked_for_removal = 0;

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
      !shm.seg[i].marked_for_removal &&
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

    // we succesfully found all the required space for shared memory mapping...

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

  // Reserve an attachment record before SHM_REMAP removes any old one.
  // get the shm adress slot like where can we stored the mapped shared segment within the code like here hmm...
  for(k = 0; k < MAX_SHM_ATTACH; k++) {
    if(!p->shmattach[k].used)
      break;
  }

  if(k == MAX_SHM_ATTACH) {
    release(&shm.lock);
    return (void*)-1;
  }

  // Allocate any needed page tables before changing existing mappings.
  // we make sure here the page table holds the entry for the given
  // we build the page table for the given adress ok means at that given slot in page directory we just create an page table with empty values
  for(j = 0; j < shm.seg[i].npages; j++) {
    if(walkpgdir(p->pgdir, (void*)(va + j * PGSIZE), 1) == 0) {
      release(&shm.lock);
      return (void*)-1;
    }
  }

  // tried to handle SHM_REMAP safely.
  if(shmflg & SHM_REMAP) {
    int overlap[MAX_SHM_ATTACH];
    int overlap_count = 0;
    int old_s;
    uint a_start, a_end;

    // Step 1: Identify overlapping shared memory attachments.
    for(k = 0; k < MAX_SHM_ATTACH; k++) {
      if(!p->shmattach[k].used)
        continue;
      // got the one shmattachment slot we can now attack in that slot....
      a_start = p->shmattach[k].va;
      a_end = a_start + p->shmattach[k].npages * PGSIZE;

      if(a_start < end && a_end > va) {
        // let's do this partial overlap is allowed
        overlap[overlap_count++] = k;
      }
    }

    // Step 2: Reject occupied pages that aren't tracked
    // shared-memory mappings.
    for(j = 0; j < shm.seg[i].npages; j++) {
      uint target = va + j * PGSIZE;
      pte_t *pte = walkpgdir(p->pgdir, (void*)target, 0);
      int covered = 0;

      if(pte && (*pte & PTE_P)) {
        for(k = 0; k < overlap_count; k++) {
          int a = overlap[k];

          //get the virtual adress of the overlapping segment like here
          a_start = p->shmattach[a].va;
          a_end = a_start + p->shmattach[a].npages * PGSIZE;

          if(target >= a_start && target < a_end) {
            covered = 1;
            break;
          }
        }

        if(!covered) {
          release(&shm.lock);
          return (void*)-1;
        }
      }
    }

    // Step 3: Remove old mappings and update old attachments.
    for(k = 0; k < overlap_count; k++) {
      int a = overlap[k];
      int old_shmid = p->shmattach[a].shmid;

      a_start = p->shmattach[a].va;

      for(j = 0; j < p->shmattach[a].npages; j++) {
        pte_t *pte = walkpgdir(
            p->pgdir, (void*)(a_start + j * PGSIZE), 0);

        if(pte)
          *pte = 0;
      }

      for(old_s = 0; old_s < NSHM; old_s++) {
        if(shm.seg[old_s].used &&
           shm.seg[old_s].shmid == old_shmid) {
          if(shm.seg[old_s].nattach > 0)
            shm.seg[old_s].nattach--;

          if(shm.seg[old_s].marked_for_removal &&
             shm.seg[old_s].nattach == 0)
            free_shm_segment(&shm.seg[old_s]);

          break;
        }
      }

      p->shmattach[a].used = 0;
      p->shmattach[a].shmid = 0;
      p->shmattach[a].va = 0;
      p->shmattach[a].npages = 0;
      p->shmattach[a].firstpage = 0;
      p->shmattach[a].shmflg = 0;
    }

    //stores the adress of the page table in the lcr3 register
    if(p == myproc())
      lcr3(V2P(p->pgdir));

  } else {
    // Without SHM_REMAP, target pages must be unused.
    for(j = 0; j < shm.seg[i].npages; j++) {
      pte_t *pte = walkpgdir(
          p->pgdir, (void*)(va + j * PGSIZE), 0);

      if(pte && (*pte & PTE_P)) {
        release(&shm.lock);
        return (void*)-1;
      }
    }
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
  p->shmattach[k].firstpage = 0;
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


// same as earlier code dead code just use to return the segment holding that physical adress initially thought for the nattach but we now moved that part to shmdt
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

//whenever an shared memory segment is removed i particularly used it for clearing the allocated pages for that shared memory initially and mainly clearing out the like shared memory segment for other enteries to use it like
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
  s->creator_name[0] = 0;
  s->mode = 0;
  s->marked_for_removal = 0;
}

// created this as seperate function because shm exit also uses it mainly during the shmexit phase like and also in the shmdt
static int
shmdt_internal(struct proc *p, uint va)
{
  int i, j, k;
  struct shmseg *s;
  pte_t *pte;

  // Find the attachment by its exact starting address. // find the actual required shared memory attachment like
  for(k = 0; k < MAX_SHM_ATTACH; k++) {
    if(p->shmattach[k].used && p->shmattach[k].va == va)
      break;
  }

  if(k == MAX_SHM_ATTACH)
    return -1;

  // Find the segment associated with this attachment.
  // find out which segment in the shared memory segment is attached to the current process like here hmm haa
  for(i = 0; i < NSHM; i++) {
    if(shm.seg[i].used &&
       shm.seg[i].shmid == p->shmattach[k].shmid)
      break;
  }

  if(i == NSHM)
    return -1;

  s = &shm.seg[i];

  // Remove this process's mappings, not the shared physical pages.
  // get the entry for that va to be removed from the hmm the page table remove the mapping
  for(j = 0; j < p->shmattach[k].npages; j++) {
    pte = walkpgdir(p->pgdir,
                    (void*)(va + j * PGSIZE), 0);

    if(pte)
      *pte = 0;
  }

  if(s->nattach > 0)
    s->nattach--;

  // Clear the process's attachment record.
  p->shmattach[k].used = 0;
  p->shmattach[k].shmid = 0;
  p->shmattach[k].va = 0;
  p->shmattach[k].npages = 0;
  p->shmattach[k].firstpage = 0;
  p->shmattach[k].shmflg = 0;

  // Physical pages are freed only after removal was requested
  // and the last attachment has disappeared.
  // actually its easy who ever is reading the code first understand all the structures and then its cake walk make diagrams or this cake will haunt you for life
  if(s->marked_for_removal && s->nattach == 0)
    free_shm_segment(s);

  return 0;
}

int
shmdt(const void *shmaddr)
{
  struct proc *p = myproc();
  uint va = (uint)shmaddr;
  int result;

  if(va % PGSIZE != 0)
    return -1;

  acquire(&shm.lock);
  result = shmdt_internal(p, va);
  release(&shm.lock);

  if(result == 0 && p->pgdir != 0)
    lcr3(V2P(p->pgdir));

  return result;
}

int
shmctl(int shmid, int cmd, void *buf)
{
  int i, highest;
  int used_segments, total_pages, total_attachments;
  struct proc *p = myproc();
  struct shmctl_ds *ds;
  struct shmctl_info *info;

  if(cmd != IPC_RMID &&
     cmd != IPC_SET &&
     cmd != IPC_STAT &&
     cmd != IPC_INFO &&
     cmd != SHM_INFO &&
     cmd != SHM_STAT &&
     cmd != SHM_STAT_ANY)
    return -1;

  // All supported commands currently require a buffer.
  if(cmd != IPC_RMID && buf == 0)
    return -1;

  acquire(&shm.lock);

  /*
   * IPC_INFO and SHM_INFO:
   * Return information about the shared-memory subsystem.
   */
  if(cmd == IPC_INFO || cmd == SHM_INFO) {
    info = (struct shmctl_info *)buf;

    used_segments = 0;
    total_pages = 0;
    total_attachments = 0;
    highest = -1;

    for(i = 0; i < NSHM; i++) {
      if(!shm.seg[i].used)
        continue;

      used_segments++;
      total_pages += shm.seg[i].npages;
      total_attachments += shm.seg[i].nattach;
      highest = i;
    }

    info->used_segments = used_segments;
    info->total_pages = total_pages;
    info->total_attachments = total_attachments;
    info->max_segments = NSHM;
    info->max_pages_per_segment = SHM_MAX_PAGES;
    info->max_total_pages = NSHM * SHM_MAX_PAGES;
    info->highest_index = highest;

    release(&shm.lock);
    return highest;
  }

  /*
   * SHM_STAT and SHM_STAT_ANY use a table index.
   * Other commands use the actual shmid.
   */
  if(cmd == SHM_STAT || cmd == SHM_STAT_ANY) {
    if(shmid < 0 || shmid >= NSHM ||
       !shm.seg[shmid].used) {
      release(&shm.lock);
      return -1;
    }

    i = shmid;
  } else {
    for(i = 0; i < NSHM; i++) {
      if(shm.seg[i].used &&
         shm.seg[i].shmid == shmid)
        break;
    }

    if(i == NSHM) {
      release(&shm.lock);
      return -1;
    }
  }

  /*
   * IPC_RMID:
   * Mark the segment for deletion.
   */
  if(cmd == IPC_RMID) {
    if(p == 0 ||
       p->pid != shm.seg[i].creator_pid ||
       shm.seg[i].marked_for_removal) {
      release(&shm.lock);
      return -1;
    }

    shm.seg[i].marked_for_removal = 1;

    if(shm.seg[i].nattach == 0)
      free_shm_segment(&shm.seg[i]);

    release(&shm.lock);
    return 0;
  }

  /*
   * Simplified permission policy:
   * The creator may inspect or modify a segment.
   * SHM_STAT_ANY allows inspection without this check.
   */
  if(cmd != SHM_STAT_ANY &&
     (p == 0 ||
      p->pid != shm.seg[i].creator_pid)) {
    release(&shm.lock);
    return -1;
  }

  /*
   * IPC_STAT, SHM_STAT, SHM_STAT_ANY:
   * Fill the metadata structure.
   */
  if(cmd == IPC_STAT ||
     cmd == SHM_STAT ||
     cmd == SHM_STAT_ANY) {
    ds = (struct shmctl_ds *)buf;

    ds->shmid = shm.seg[i].shmid;
    ds->key = shm.seg[i].key;
    ds->size = shm.seg[i].size;
    ds->npages = shm.seg[i].npages;
    ds->nattach = shm.seg[i].nattach;
    ds->creator_pid = shm.seg[i].creator_pid;
    safestrcpy(ds->creator_name, shm.seg[i].creator_name,
               sizeof(ds->creator_name));
    ds->mode = shm.seg[i].mode;
    ds->marked_for_removal = shm.seg[i].marked_for_removal;

    shmid = shm.seg[i].shmid;

    release(&shm.lock);

    if(cmd == IPC_STAT)
      return 0;

    return shmid;
  }

  /*
   * IPC_SET:
   * Update the segment's permission bits.
   */
  if(cmd == IPC_SET) {
    ds = (struct shmctl_ds *)buf;
    shm.seg[i].mode = ds->mode & 0777;

    release(&shm.lock);
    return 0;
  }

  release(&shm.lock);
  return -1;
}

void
shmexit(struct proc *p)
{
  int k;

  acquire(&shm.lock);

  for(k = 0; k < MAX_SHM_ATTACH; k++) {
    if(!p->shmattach[k].used)
      continue;

    // Reuse the same cleanup logic as shmdt().
    if(shmdt_internal(p, p->shmattach[k].va) < 0) {
      // Clear stale attachment metadata if cleanup fails.
      p->shmattach[k].used = 0;
      p->shmattach[k].shmid = 0;
      p->shmattach[k].va = 0;
      p->shmattach[k].npages = 0;
      p->shmattach[k].firstpage = 0;
      p->shmattach[k].shmflg = 0;
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
      // as soon as i get an free slot i break down ok
      if(!child->shmattach[i].used)
        break;
    }

    if(i == MAX_SHM_ATTACH){
      cprintf("shmfork: no free child attachment slot\n");
      goto bad;
    }
    // Find the shared-memory segment from the shmget like ok
    // dekho konsa shared memory segment attached hain ok got it haa
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
    //k mein parent ka attachment
    //i mein child ka attachments
    // j mein shm ka actual shared memory segment

    // Verify that the child's corresponding pages exist
    // and are not already mapped.
    for(j = 0; j < parent->shmattach[k].npages; j++) {
      pte = walkpgdir(parent->pgdir,
                      (void*)(va + j * PGSIZE), 0);
      cpte = walkpgdir(child->pgdir,
                       (void*)(va + j * PGSIZE), 1);

      if(pte == 0 || !(*pte & PTE_P)) {
          cprintf("shmfork: parent PTE missing at 0x%x\n",
          va + j * PGSIZE);
          goto bad;
      }

      if(cpte == 0) {
        cprintf("shmfork: child page-table allocation failed\n");
        goto bad;
      }

      // i expected ki child mein koi bhi entry nahi chahiya right i am making it right now here so...............
      if(*cpte & PTE_P) {
        cprintf("shmfork: child PTE already present at 0x%x\n",
        va + j * PGSIZE);
        goto bad;
      }
    }

    // Map the same physical shared pages into the child.
    for(j = 0; j < parent->shmattach[k].npages; j++) {
      pte = walkpgdir(parent->pgdir,(void*)(va + j * PGSIZE), 0);
      cpte = walkpgdir(child->pgdir,(void*)(va + j * PGSIZE), 0);

      *cpte = PTE_ADDR(*pte) | (PTE_FLAGS(*pte));
    }

    // Record the inherited attachment.
    child->shmattach[i].used = 1;
    child->shmattach[i].shmid = parent->shmattach[k].shmid;
    child->shmattach[i].va = va;
    child->shmattach[i].npages = parent->shmattach[k].npages;
    child->shmattach[i].firstpage = parent->shmattach[k].firstpage;
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

      for(j = 0; j < child->shmattach[k].npages; j++) {
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
    child->shmattach[k].firstpage = 0;
    child->shmattach[k].shmflg = 0;
  }

  release(&shm.lock);
  return -1;
}

void*
shm_map(int shmid, const void *hint, uint length, uint offset,
        int prot, int flags)
{
  int i, j, k, npages, perm, found = 0;
  uint va, mapbytes, firstpage;
  struct proc *p = myproc();
  pte_t *pte;

  if(length == 0 || length > 0xffffffffU - (PGSIZE - 1) ||
     offset % PGSIZE ||
     (flags != MAP_SHARED && flags != (MAP_SHARED | MAP_FIXED)) ||
     (prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC)))
    return (void*)-1;

  acquire(&shm.lock);
  for(i = 0; i < NSHM; i++)
    if(shm.seg[i].used && !shm.seg[i].marked_for_removal &&
       shm.seg[i].shmid == shmid)
      break;
  if(i == NSHM || offset >= (uint)shm.seg[i].size ||
     length > (uint)shm.seg[i].size - offset){
    release(&shm.lock);
    return (void*)-1;
  }

  firstpage = offset / PGSIZE;
  mapbytes = PGROUNDUP(length);
  npages = mapbytes / PGSIZE;
  if(firstpage + npages > (uint)shm.seg[i].npages){
    release(&shm.lock);
    return (void*)-1;
  }

  for(k = 0; k < MAX_SHM_ATTACH && p->shmattach[k].used; k++)
    ;
  if(k == MAX_SHM_ATTACH){
    release(&shm.lock);
    return (void*)-1;
  }

  va = (uint)hint;
  if(flags & MAP_FIXED){
    if(va % PGSIZE){
      release(&shm.lock);
      return (void*)-1;
    }
  } else if(va != 0){
    va = PGROUNDDOWN(va);
  }

  if(va >= SHMBASE && va + mapbytes >= va && va + mapbytes <= SHMLIMIT){
    found = 1;
    for(j = 0; j < npages; j++){
      pte = walkpgdir(p->pgdir, (void*)(va + j * PGSIZE), 0);
      if(pte && (*pte & PTE_P)){
        found = 0;
        break;
      }
    }
  }
  if(!found && (flags & MAP_FIXED)){
    release(&shm.lock);
    return (void*)-1;
  }
  if(!found){
    for(va = SHMBASE; va + mapbytes >= va && va + mapbytes <= SHMLIMIT;
        va += PGSIZE){
      for(j = 0; j < npages; j++){
        pte = walkpgdir(p->pgdir, (void*)(va + j * PGSIZE), 0);
        if(pte && (*pte & PTE_P))
          break;
      }
      if(j == npages){
        found = 1;
        break;
      }
    }
  }
  if(!found){
    release(&shm.lock);
    return (void*)-1;
  }
  for(j = 0; j < npages; j++){
    if(walkpgdir(p->pgdir, (void*)(va + j * PGSIZE), 1) == 0){
      release(&shm.lock);
      return (void*)-1;
    }
  }

  perm = PTE_U;
  if(prot & PROT_WRITE)
    perm |= PTE_W;
  for(j = 0; j < npages; j++){
    if(mappages(p->pgdir, (void*)(va + j * PGSIZE), PGSIZE,
                V2P(shm.seg[i].pages[firstpage + j]), perm) < 0){
      while(j > 0){
        pte = walkpgdir(p->pgdir, (void*)(va + --j * PGSIZE), 0);
        if(pte)
          *pte = 0;
      }
      lcr3(V2P(p->pgdir));
      release(&shm.lock);
      return (void*)-1;
    }
  }

  p->shmattach[k].used = 1;
  p->shmattach[k].shmid = shmid;
  p->shmattach[k].va = va;
  p->shmattach[k].npages = npages;
  p->shmattach[k].firstpage = firstpage;
  p->shmattach[k].shmflg = (prot & PROT_WRITE) ? 0 : SHM_RDONLY;
  shm.seg[i].nattach++;
  lcr3(V2P(p->pgdir));
  release(&shm.lock);
  return (void*)va;
}
