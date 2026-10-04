#ifndef SHM_H
#define SHM_H
#include "spinlock.h"
/*Flags for shmget*/
#define IPC_CREAT 01000

/*General limits for sengment*/
#define NSHM 16
#define SHM_MAX_PAGES 16

/*flags for `shmat`*/
#define SHM_RDONLY	010000		/* attach read-only else read-write */
#define SHM_RND		020000		/* round attach address to SHMLBA */
#define SHM_REMAP	040000		/* take-over region on attach */
#define SHM_EXEC	0100000		/* execution access */

struct shmseg {
  int used;
  int key;
  int shmid;
  int size;
  int npages;
  int nattach;
  char *pages[SHM_MAX_PAGES];
};

struct {
  struct spinlock lock;
  struct shmseg seg[NSHM];
} shm;
#endif
