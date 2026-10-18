#ifndef SHM_H
#define SHM_H

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

#define IPC_RMID      0
#define IPC_SET       1
#define IPC_STAT      2
#define IPC_INFO      3

#define SHM_STAT      13
#define SHM_INFO      14
#define SHM_STAT_ANY  15

struct shmctl_ds {
  int shmid;
  int key;
  int size;
  int npages;
  int nattach;
  int creator_pid;
  int mode;
  int marked_for_removal;
};

struct shmctl_info {
  int max_segments;
  int max_pages_per_segment;
  int max_total_pages;
  int used_segments;
  int total_pages;
  int total_attachments;
  int highest_index;
};

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
  int mode;
  int marked_for_removal;
};

struct shmid_ds {
  int shm_segsz;   // Segment size in bytes
  int shm_nattch;  // Number of active attachments
  int shm_cpid;    // PID of creator
  int shm_perm_mode; // Permission bits
};

void shminit(void);
int shmget(int, int, int);
void* shmat(int, const void*, int);
void shmexit(struct proc*);
int shmfork(struct proc*, struct proc*);

int is_shared_page(uint);
int get_shm_segment(uint);
int shmdt(const void *);
int shmctl(int shmid, int cmd, void *buf);
// int shmctl(int shmid, int cmd, struct shmid_ds *buf);

#endif
