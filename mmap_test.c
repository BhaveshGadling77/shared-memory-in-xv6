#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "shm.h"

#define TEST_PGSIZE 4096

int
main(int argc, char *argv[])
{
  int fd, fd2, fdro, passed, failed, pid;
  void *addr, *addr2;

  passed = failed = 0;

  // An invalid descriptor must be rejected!!
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

  fd = shm_open("/mmapobj", O_CREAT | O_RDWR, 0600);
  fd2 = shm_open("/mmapobj", O_RDWR, 0600);
  if(fd < 0 || fd2 < 0 || ftruncate(fd, 2 * TEST_PGSIZE) < 0){
    printf(1, "mmap.shared-object setup fail\n");
    failed++;
    if(fd >= 0) close(fd);
    if(fd2 >= 0) close(fd2);
    unlink("/mmapobj");
  } else {
    addr = mmap(0, 2 * TEST_PGSIZE, PROT_READ | PROT_WRITE,
                MAP_SHARED, fd, 0);
    addr2 = mmap(0, 2 * TEST_PGSIZE, PROT_READ | PROT_WRITE,
                 MAP_SHARED, fd2, 0);
    if(addr != (void*)-1 && addr2 != (void*)-1){
      ((char*)addr)[0] = 'S';
      ((char*)addr2)[TEST_PGSIZE] = 'T';
      if(((char*)addr2)[0] == 'S' &&
         ((char*)addr)[TEST_PGSIZE] == 'T'){
        printf(1, "mmap.same-object-pages pass\n");
        passed++;
      } else {
        printf(1, "mmap.same-object-pages fail\n");
        failed++;
      }

    pid = fork();
    if(pid == 0){
      ((char*)addr)[1] = 'F';
      exit();
    }
    if(pid > 0 && wait() == pid && ((char*)addr2)[1] == 'F'){
      printf(1, "mmap.fork-shared-page pass\n");
      passed++;
    } else {
      printf(1, "mmap.fork-shared-page fail\n");
      failed++;
    }
    } else {
      printf(1, "mmap.shared-object map fail\n");
      failed++;
    }

    if(ftruncate(fd, -1) < 0 && ftruncate(-1, 4096) < 0){
      printf(1, "ftruncate.invalid-arguments pass\n");
      passed++;
    } else {
      printf(1, "ftruncate.invalid-arguments fail\n");
      failed++;
    }

    fdro = shm_open("/mmapobj", 0, 0);
    if(ftruncate(fd, SHM_MAX_PAGES * TEST_PGSIZE + 1) < 0 &&
       fdro >= 0 && ftruncate(fdro, TEST_PGSIZE) < 0){
      printf(1, "ftruncate.limit-and-readonly pass\n");
      passed++;
    } else {
      printf(1, "ftruncate.limit-and-readonly fail\n");
      failed++;
    }
    if(fdro >= 0) close(fdro);

    if(ftruncate(fd, TEST_PGSIZE) == 0 &&
       mmap(0, 2 * TEST_PGSIZE, PROT_READ, MAP_SHARED, fd, 0) == (void*)-1 &&
       ftruncate(fd, 2 * TEST_PGSIZE) == 0 &&
       ((char*)addr2)[TEST_PGSIZE] == 0){
      printf(1, "ftruncate.shrink-grow pass\n");
      passed++;
    } else {
      printf(1, "ftruncate.shrink-grow fail\n");
      failed++;
    }

    unlink("/mmapobj");
    close(fd);
    close(fd2);
    fd = fd2 = -1;
    if(addr != (void*)-1 && addr2 != (void*)-1 &&
       ((char*)addr)[0] == 'S'){
      printf(1, "mmap.close-unlink-live-mapping pass\n");
      passed++;
    } else {
      printf(1, "mmap.close-unlink-live-mapping fail\n");
      failed++;
    }
    if(addr != (void*)-1) shmdt(addr);
    if(addr2 != (void*)-1) shmdt(addr2);
  }

  printf(1, "mmap tests: %d passed, %d failed\n", passed, failed);
  exit();
}
