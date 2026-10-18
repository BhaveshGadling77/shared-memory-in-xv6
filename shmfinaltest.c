#include "types.h"
#include "user.h"
#include "shm.h"

#define PGSIZE_TEST 4096

int passed = 0;
int failed = 0;

void
check(char *name, int result)
{
  if(result) {
    printf(1, "PASS: %s\n", name);
    passed++;
  } else {
    printf(1, "FAIL: %s\n", name);
    failed++;
  }
}

int
isbad(void *p)
{
  return p == (void *)-1 || p == 0;
}

int
main(void)
{
  int key, id, id2, ret, pid, w;
  char *p, *q, *r;
  struct shmctl_ds ds;
  struct shmctl_info info;

  key = 0x530000 + getpid();

  printf(1, "Testing shared memory\n\n");

  id = shmget(key, 2 * PGSIZE_TEST, IPC_CREAT | 0600);
  check("shmget creates segment", id > 0);

  if(id < 0) {
    printf(1, "Cannot continue without a segment\n");
    exit();
  }

  id2 = shmget(key, 2 * PGSIZE_TEST, 0);
  check("shmget finds existing key", id2 == id);

  ret = shmget(key, 2 * PGSIZE_TEST, IPC_CREAT | IPC_EXCL | 0600);
  check("IPC_EXCL rejects existing key", ret == -1);

  ret = shmget(key + 1, PGSIZE_TEST, 0);
  check("shmget rejects missing key", ret == -1);

  ret = shmget(key + 2, 0, IPC_CREAT | 0600);
  check("shmget rejects zero size", ret == -1);

  ret = shmget(key + 3, -1, IPC_CREAT | 0600);
  check("shmget rejects negative size", ret == -1);

  ret = shmget(key + 4, 17 * PGSIZE_TEST, IPC_CREAT | 0600);
  check("shmget rejects oversized segment", ret == -1);

  p = (char *)shmat(id, 0, 0);
  check("shmat attaches segment", !isbad(p));

  if(isbad(p)) {
    shmctl(id, IPC_RMID, 0);
    exit();
  }

  q = (char *)shmat(id, 0, 0);
  check("second attachment succeeds", !isbad(q));

  if(!isbad(q)) {
    p[0] = 'A';
    check("data shared between attachments", q[0] == 'A');

    p[PGSIZE_TEST] = 'B';
    check("second page is shared",
          q[PGSIZE_TEST] == 'B');

    check("detach second attachment", shmdt(q) == 0);
  }

  check("detach rejects wrong base address",
        shmdt(p + 1) == -1);

  check("shmat rejects invalid ID",
        shmat(-1, 0, 0) == (void *)-1);

  check("shmat rejects invalid flags",
        shmat(id, 0, 0x40000000) == (void *)-1);

  check("shmdt succeeds",
        shmdt(p) == 0);

  ret = shmctl(id, IPC_STAT, &ds);
  check("IPC_STAT succeeds", ret == 0);

  if(ret == 0) {
    check("IPC_STAT reports correct size",
          ds.size == 2 * PGSIZE_TEST);

    check("IPC_STAT reports zero attachments",
          ds.nattach == 0);

    check("IPC_STAT reports segment metadata",
          ds.shmid == id && ds.key == key &&
          ds.npages == 2 && ds.creator_pid == getpid() &&
          ds.marked_for_removal == 0);
  }

  check("IPC_STAT rejects null buffer",
        shmctl(id, IPC_STAT, 0) == -1);

  check("IPC_STAT rejects invalid ID",
        shmctl(-1, IPC_STAT, &ds) == -1);

  check("shmctl rejects invalid command",
        shmctl(id, 999, &ds) == -1);

  ds.mode = 0640;
  ret = shmctl(id, IPC_SET, &ds);
  check("IPC_SET changes permissions", ret == 0);

  ret = shmctl(id, IPC_STAT, &ds);
  check("IPC_STAT reads updated permissions",
        ret == 0 && ds.mode == 0640);

  ret = shmctl(id, IPC_INFO, &info);
  check("IPC_INFO returns valid index", ret >= 0);

  check("IPC_INFO reports limits",
        info.max_segments > 0 &&
        info.max_pages_per_segment > 0 &&
        info.max_total_pages == info.max_segments *
                                info.max_pages_per_segment);

  ret = shmctl(id, SHM_INFO, &info);
  check("SHM_INFO returns valid index", ret >= 0);

  check("SHM_INFO reports active segment",
        info.used_segments > 0);

  check("SHM_INFO reports allocated pages",
        info.total_pages >= 2);

  check("SHM_INFO reports attachments and highest index",
        info.total_attachments >= 0 &&
        info.highest_index >= 0);

  ret = shmctl(id - 1, SHM_STAT, &ds);
  check("SHM_STAT looks up table index", ret == id);

  ret = shmctl(id - 1, SHM_STAT_ANY, &ds);
  check("SHM_STAT_ANY looks up table index", ret == id);

  check("SHM_STAT rejects invalid index",
        shmctl(-1, SHM_STAT, &ds) == -1);

  p = (char *)shmat(id, 0, 0);
  check("attach before fork", !isbad(p));

  if(!isbad(p)) {
    p[0] = 'P';
    pid = fork();

    if(pid == 0) {
      p[0] = 'C';
      exit();
    } else if(pid < 0) {
      check("fork succeeds", 0);
    } else {
      w = wait();
      check("wait reaps child", w == pid);
      check("child changes visible to parent", p[0] == 'C');
      check("parent detaches after fork", shmdt(p) == 0);
    }
  }

  p = (char *)shmat(id, 0, 0);
  check("attach before read-only test", !isbad(p));

  if(!isbad(p)) {
    p[0] = 'R';
    shmdt(p);
  }

  r = (char *)shmat(id, 0, SHM_RDONLY);
  check("read-only attachment succeeds", !isbad(r));

  if(!isbad(r)) {
    check("read-only mapping allows reading", r[0] == 'R');

    pid = fork();

    if(pid == 0) {
      r[0] = 'X';
      exit();
    } else if(pid < 0) {
      check("fork for read-only test", 0);
    } else {
      w = wait();
      check("read-only child is reaped", w == pid);
      check("read-only write does not change data",
            r[0] == 'R');
      check("detach read-only mapping", shmdt(r) == 0);
    }
  }

  p = (char *)shmat(id, 0, 0);
  check("attach before IPC_RMID", !isbad(p));

  if(!isbad(p)) {
    p[0] = 'D';

    ret = shmctl(id, IPC_RMID, 0);
    check("IPC_RMID marks attached segment", ret == 0);

    check("existing attachment remains usable", p[0] == 'D');

    check("removed segment cannot be attached",
          shmat(id, 0, 0) == (void *)-1);

    check("removed key cannot be found",
          shmget(key, PGSIZE_TEST, 0) == -1);

    check("final detach succeeds", shmdt(p) == 0);
  }

  check("removed segment no longer exists",
        shmctl(id, IPC_STAT, &ds) == -1);

  id2 = shmget(key + 10, PGSIZE_TEST, IPC_CREAT | 0600);
  check("create segment for immediate removal", id2 > 0);

  if(id2 > 0) {
    check("IPC_RMID removes unattached segment",
          shmctl(id2, IPC_RMID, 0) == 0);

    check("immediately removed segment is gone",
          shmctl(id2, IPC_STAT, &ds) == -1);
  }

  id = shmget(key + 20, PGSIZE_TEST, IPC_CREAT | 0600);
  id2 = shmget(key + 21, PGSIZE_TEST, IPC_CREAT | 0600);
  p = (char *)shmat(id, (void *)0x70020000, 0);
  check("attach first segment for SHM_REMAP", !isbad(p));
  if(!isbad(p)) {
    p[0] = 'M';
    q = (char *)shmat(id2, (void *)0x70020000, SHM_REMAP);
    check("SHM_REMAP replaces the complete attachment",
          !isbad(q));
    if(!isbad(q)) {
      check("SHM_REMAP maps the replacement segment", q[0] == 0);
      check("detach remapped segment", shmdt(q) == 0);
    }
  }
  check("remove SHM_REMAP test segments",
        shmctl(id, IPC_RMID, 0) == 0 &&
        shmctl(id2, IPC_RMID, 0) == 0);

  printf(1, "\nPassed: %d\n", passed);
  printf(1, "Failed: %d\n", failed);

  if(failed == 0)
    printf(1, "All tests passed\n");
  else
    printf(1, "Some tests failed\n");

  exit();
}
