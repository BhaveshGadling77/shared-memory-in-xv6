#include "types.h"
#include "user.h"
#include "shm.h"

int
main(void)
{
  int id;
  char *p;

  id = shmget(5678, 4096, IPC_CREAT);
  if(id < 0) {
    printf(1, "shmget failed\n");
    exit();
  }

  // Deliberately unaligned address.
  p = (char*)shmat(id, (void*)0x7000107B, SHM_RND);

  if(p == (char*)-1) {
    printf(1, "SHM_RND test FAILED: shmat rejected address\n");
  } else {
    printf(1, "SHM_RND returned address: 0x%x\n", (uint)p);

    if((uint)p <= 0x7000107B)
      printf(1, "Check rounded-down address\n");
    else
      printf(1, "Unexpected address\n");
  }

  exit();
}