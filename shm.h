#ifndef SHM_H
#define SHM_H

#include "spinlock.h"

struct proc;

#define IPC_PRIVATE 0
#define IPC_CREAT 01000
#define IPC_EXCL  02000

#define NSHM 16
#define SHM_MAX_PAGES 16
#define MAX_SHM_ATTACH 16

#define SHM_RDONLY 010000
#define SHM_RND    020000
#define SHM_REMAP  040000
#define SHM_EXEC   0100000

#define SHM_ALLOWED_FLAGS \
  (SHM_RDONLY | SHM_RND | SHM_REMAP | SHM_EXEC)

struct shmseg {
  int used;
  int key;
  int shmid;
  int size;
  int npages;
  int nattach;
  int creator_pid;
  char *pages[SHM_MAX_PAGES];
};

struct shmstate {
  struct spinlock lock;
  struct shmseg seg[NSHM];
};

extern struct shmstate shm;

void shminit(void);
int shmget(int, int, int);
void* shmat(int, const void*, int);
void shmexit(struct proc*);
int shmfork(struct proc*, struct proc*);

int is_shared_page(uint);
int get_shm_segment(uint);

#endif