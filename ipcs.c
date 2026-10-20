#include "types.h"
#include "user.h"
#include "shm.h"

void usage(void)
{
  printf(1, "usage: ipcs [-a|-m|-q|-s] [-c|-l|-p|-t|-u] [-i id]\n\n");
  printf(1, "xv6 supports shared memory only (-m); queues and semaphores are unavailable.\n\n");
}

int option(char *arg)
{
  if(strcmp(arg, "--help") == 0) 
    return 'h';
  if(strcmp(arg, "--version") == 0)
    return 'V';
  if(strcmp(arg, "--all") == 0)
    return 'a';
  if(strcmp(arg, "--shmems") == 0)
    return 'm';
  if(strcmp(arg, "--queues") == 0)
    return 'q';
  if(strcmp(arg, "--semaphores") == 0)
    return 's';
  if(strcmp(arg, "--creator") == 0)
    return 'c';
  if(strcmp(arg, "--limits") == 0) 
    return 'l';
  if(strcmp(arg, "--pid") == 0)
    return 'p';
  if(strcmp(arg, "--time") == 0)
    return 't';
  if(strcmp(arg, "--summary") == 0)
    return 'u';
  if(strcmp(arg, "--bytes") == 0)
    return 'b';
  if(strcmp(arg, "--human") == 0)
    return 'H';
  if(strcmp(arg, "--id") == 0) 
    return 'i';
  
  if(arg[0] == '-' && arg[1] != '-' && arg[2] == 0)
    return arg[1];
  return 0;
}

int main(int argc, char **argv)
{
  int i, id, resource, format, bytes, human, show_id;
  struct shmctl_ds ds;
  struct shmctl_info info;

  resource = 0; //expect all resource will be available.
  format = 0;
  bytes = 0;
  human = 0;
  show_id = 0;
  id = -1;

  for(i = 1; i < argc; i++) {
    switch(option(argv[i])) {
      case 'h':
        usage();
        exit();
      case 'V':
        printf(1, "ipcs (xv6) 1.0\n\n");
        exit();
      case 'a':
      case 'm':
        resource = 1;
        break;
      case 'q':
      case 's':
        printf(1, "ipcs: message queues and semaphore arrays are not supported by xv6\n");
        exit();
      case 'c':
        format = 1;
        break;
      case 'l':
        format = 2;
        break;
      case 'p':
        format = 3;
        break;
      case 't':
        format = 4;
        break;
      case 'u':
        format = 5;
        break;
      case 'b':
        bytes = 1;
        break;
      case 'H':
        human = 1;
        break;
      case 'i':
        if(i + 1 >= argc) {
          usage();
          exit();
        }
        id = atoi(argv[++i]);
        if(id <= 0) {
          printf(1, "ipcs: invalid shared memory id\n");
          exit();
        }
        show_id = 1;
        break;
      default:
        usage();
        exit();
    }
  }

  if(show_id && (resource != 1 || id <= 0)) {
    printf(1, "ipcs: -i requires -m or -a and a valid id\n");
    exit();
  }
  if(resource == 0)
    printf(1, "ipcs: queues and semaphores are unavailable; showing shared memory only\n");

  if(format == 2 || format == 5) {
    if(shmctl(0, SHM_INFO, &info) < 0) {
      printf(1, "ipcs: cannot get shared memory information\n");
      exit();
    }
    if(format == 2) {
      printf(1, " Shared Memory Limits \n");
      printf(1, "max segments = %d\n", info.max_segments);
      printf(1, "max pages per segment = %d\n", info.max_pages_per_segment);
      printf(1, "max total pages = %d (%d bytes)\n", info.max_total_pages,
             info.max_total_pages * 4096);
      if(bytes || human)
        printf(1, "unit = bytes\n");
    } else {
      printf(1, " Shared Memory Status \n");
      printf(1, "%d segments, %d pages allocated, %d attachments\n",
             info.used_segments, info.total_pages, info.total_attachments);
    }
    exit();
  }

  if(show_id) {
    if(shmctl(id - 1, SHM_STAT_ANY, &ds) < 0 || ds.shmid != id) {
      printf(1, "ipcs: shared memory id %d not found or not accessible\n", id);
      exit();
    }
    printf(1, "\n Shared Memory Segment \n");
    printf(1, "key        shmid      owner      perms      bytes      nattch\n");
    printf(1, "0x%x     %d          %d          %o          %d          %d\n",
           ds.key, ds.shmid, ds.creator_pid, ds.mode, ds.size, ds.nattach);
    printf(1, "pages = %d, marked for removal = %d\n", ds.npages, ds.marked_for_removal);
    exit();
  }

  printf(1, "Shared Memory Segments\n");
  if(format == 1)
    printf(1, "key        shmid      owner      creator\n");
  else if(format == 3)
    printf(1, "key        shmid      owner      cpid\n");
  else if(format == 4)
    printf(1, "key        shmid      owner      perms      bytes      nattch\n");
  else
    printf(1, "key        shmid      owner      perms      bytes      nattch\n");

  for(i = 0; i < NSHM; i++) {
    if(shmctl(i, SHM_STAT_ANY, &ds) < 0)
      continue;
    if(format == 1)
      printf(1, "0x%x     %d          %d          %d\n\n", ds.key, ds.shmid, ds.creator_pid, ds.creator_pid);
    else if(format == 3)
      printf(1, "0x%x     %d          %d          %d\n\n", ds.key, ds.shmid, ds.creator_pid, ds.creator_pid);
    else
      printf(1, "0x%x     %d          %d          %o          %d          %d\n\n",
             ds.key, ds.shmid, ds.creator_pid, ds.mode, ds.size, ds.nattach);
  }
  exit();
}
