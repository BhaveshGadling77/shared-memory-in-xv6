#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "shm.h"

#define TEST_PGSIZE 4096

int
main(int argc, char *argv[])
{
  int fd, passed, failed;
  void *addr;

  passed = failed = 0;

  // An invalid descriptor must be rejected.
  addr = mmap(0, TEST_PGSIZE, PROT_READ | PROT_WRITE,
              MAP_SHARED, -1, 0);
  if(addr == (void*)-1){
    printf(1, "mmap.invalid-fd pass\n");
    passed++;
  } else {
    printf(1, "mmap.invalid-fd fail\n");
    failed++;
  }

  // A regular file is not a POSIX shared-memory object descriptor.
  fd = open("README", O_RDONLY);
  if(fd < 0){
    printf(1, "mmap.setup fail: could not open README\n");
    failed++;
  } else {
    addr = mmap(0, TEST_PGSIZE, PROT_READ, MAP_SHARED, fd, 0);
    if(addr == (void*)-1){
      printf(1, "mmap.non-shm-fd pass\n");
      passed++;
    } else {
      printf(1, "mmap.non-shm-fd fail\n");
      failed++;
    }
    close(fd);
  }

  // The syscall rejects zero-length mappings.
  fd = open("README", O_RDONLY);
  if(fd >= 0){
    addr = mmap(0, 0, PROT_READ, MAP_SHARED, fd, 0);
    if(addr == (void*)-1){
      printf(1, "mmap.zero-length pass\n");
      passed++;
    } else {
      printf(1, "mmap.zero-length fail\n");
      failed++;
    }
    close(fd);
  }

  printf(1, "mmap tests: %d passed, %d failed\n", passed, failed);
  exit();
}
