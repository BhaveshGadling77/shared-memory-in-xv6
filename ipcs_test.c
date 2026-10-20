#include "types.h"
#include "user.h"
#include "shm.h"

void run(char *a, char *b, char *c)
{
  int pid, argc;
  char *argv[5];

  argc = 0;
  argv[argc++] = "ipcs";
  if(a) argv[argc++] = a;
  if(b) argv[argc++] = b;
  if(c) argv[argc++] = c;
  argv[argc] = 0;

  pid = fork();
  if(pid == 0) {
    exec("ipcs", argv);
    printf(2, "ipcs_test: exec ipcs failed\n");
    exit();
  }
  if(pid > 0)
    wait();
}

int main(void)
{
  int id, n, pos;
  char idtext[12];

  id = shmget(0x49504353, 4096, IPC_CREAT | 0600);
  if(id < 0) {
    printf(2, "ipcs_test: unable to create test segment\n");
    exit();
  }

  n = id;
  pos = 0;
  while(n > 0) {
    idtext[pos++] = '0' + n % 10;
    n /= 10;
  }

  idtext[pos] = 0;
  for(n = 0; n < pos / 2; n++) {
    char c = idtext[n];
    idtext[n] = idtext[pos - n - 1];
    idtext[pos - n - 1] = c;
  }

  run("-h", 0, 0);
  run("--help", 0, 0);
  run("-V", 0, 0);
  run("--version", 0, 0);
  run("-a", 0, 0);
  run("--all", 0, 0);
  run("-m", 0, 0);
  run("--shmems", 0, 0);
  run("-q", 0, 0);
  run("--queues", 0, 0);
  run("-s", 0, 0);
  run("--semaphores", 0, 0);
  run("-c", 0, 0);
  run("--creator", 0, 0);
  run("-l", 0, 0);
  run("--limits", 0, 0);
  run("-p", 0, 0);
  run("--pid", 0, 0);
  run("-t", 0, 0);
  run("--time", 0, 0);
  run("-u", 0, 0);
  run("--summary", 0, 0);
  run("-m", "-b", 0);
  run("-m", "--bytes", 0);
  run("-m", "--human", 0);
  run("-l", "-b", 0);
  run("-l", "--human", 0);
  run("-m", "-i", idtext); 
  run("-a", "--id", idtext);

  shmctl(id, IPC_RMID, 0);
  printf(1, "ipcs_test: smoke test complete\n");
  exit();
}
