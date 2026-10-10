#include "types.h"
#include "user.h"
#include "shm.h"

int
main(void)
{
  int id;
  char *p;

  id = shmget(5679, 4096, IPC_CREAT);
  if(id < 0) {
    printf(1, "shmget failed\n");
    exit();
  }

  p = (char*)shmat(id, 0, SHM_EXEC);

  if(p == (char*)-1) {
    printf(1, "SHM_EXEC test FAILED: shmat rejected flag\n");
  } else {
    printf(1, "SHM_EXEC mapping accepted at 0x%x\n", (uint)p);
    printf(1, "This confirms acceptance only, not execution permission.\n");
  }

  exit();
}
