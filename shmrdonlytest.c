#include "types.h"
#include "user.h"
#include "shm.h"

int
main(void)
{
  int id;
  int *p;

  id = shmget(5680, sizeof(int), IPC_CREAT);
  if(id < 0) {
    printf(1, "shmget failed\n");
    exit();
  }

  p = (int*)shmat(id, 0, 0);
  if(p == (int*)-1) {
    printf(1, "Initial shmat failed\n");
    exit();
  }

  *p = 123;
  printf(1, "Initial value = %d\n", *p);

  // Detach is not implemented yet, so use another process
  // to test read-only protection on a fresh mapping.
  if(fork() == 0) {
    int *r = (int*)shmat(id, (void*)0x70010000, SHM_RDONLY);

    if(r == (int*)-1) {
      printf(1, "Read-only shmat failed\n");
      exit();
    }

    printf(1, "Read-only mapping reads = %d\n", *r);
    printf(1, "Attempting write; protection fault expected\n");
    *r = 456;

    printf(1, "ERROR: write unexpectedly succeeded\n");
    exit();
  }

  wait();
  exit();
}
