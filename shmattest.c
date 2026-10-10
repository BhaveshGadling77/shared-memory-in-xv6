#include "types.h"
#include "stat.h"
#include "user.h"
#include "shm.h"

#define SHM_KEY 1234

int
main(void)
{
  int shmid;
  int pid;
  int *shared;

  printf(1, "Creating shared memory...\n");

  shmid = shmget(SHM_KEY, sizeof(int), IPC_CREAT);

  if(shmid < 0) {
    printf(1, "shmget failed\n");
    exit();
  }

  printf(1, "Shared memory id = %d\n", shmid);

  shared = (int*)shmat(shmid, 0, 0);

  if(shared == (int*)-1) {
    printf(1, "Parent shmat failed\n");
    exit();
  }

  //parent shared value
  *shared = 0;

  printf(1, "Parent: initialized shared = %d\n", *shared);

  pid = fork();

  if(pid < 0) {
    printf(1, "fork failed\n");
    exit();
  }


if(pid == 0) {
  *shared = 100;
  printf(1, "Child: wrote shared = %d\n", *shared);
  exit();
} else {
  sleep(10);
  printf(1, "Parent: after child write = %d\n", *shared);

  *shared = 200;
  printf(1, "Parent: wrote shared = %d\n", *shared);

  wait();
  printf(1, "Parent: final shared = %d\n", *shared);
}

  
  exit();
  
}
