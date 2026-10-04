#include "types.h"
#include "stat.h"
#include "user.h"
#include "shm.h"

#define IPC_CREAT 01000

int
main(void)
{
  int shmid;

  printf(1, "Testing shmget...\n");

  shmid = shmget(1234, 4096, IPC_CREAT);

  if(shmid < 0){
    printf(1, "shmget failed\n");
    exit();
  }

  printf(1, "shmget successful\n");
  printf(1, "Returned shmid = %d\n", shmid);

  exit();
}
