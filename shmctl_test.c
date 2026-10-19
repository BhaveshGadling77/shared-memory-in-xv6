#include "types.h"
#include "user.h"
#include "shm.h"

int passed = 0;
int failed = 0;
char testname[40];

void check(char *name, int result) {
    strcpy(testname, name);
    if (result) {
        printf(1, "%s pass\n", testname);
        passed++;
    } else {
        printf(1, "%s fail\n", testname);
        failed++;
    }
}

int main(void) {
    int id, ret;
    struct shmctl_ds ds;
    struct shmctl_info info;

    id = shmget(0x560000 + getpid(), 8192, IPC_CREAT | 0600);
    check("shmctl.1 create segment", id >= 0);

    if (id < 0)
        exit();

    ret = shmctl(id, IPC_STAT, &ds);
    check("shmctl.2 IPC_STAT", ret == 0);
    if (ret == 0) {
        check("shmctl.3 segment size", ds.size == 8192);
        check("shmctl.4 segment metadata",
              ds.shmid == id && ds.npages == 2 &&
              ds.creator_pid == getpid() && ds.marked_for_removal == 0);
    }

    check("shmctl.5 reject null IPC_STAT buffer",
          shmctl(id, IPC_STAT, 0) == -1);
    check("shmctl.6 reject invalid ID",
          shmctl(-1, IPC_STAT, &ds) == -1);
    check("shmctl.7 reject invalid command",
          shmctl(id, 999, &ds) == -1);

    ds.mode = 0640;
    ret = shmctl(id, IPC_SET, &ds);
    check("shmctl.8 IPC_SET", ret == 0);

    ret = shmctl(id, IPC_STAT, &ds);
    check("shmctl.9 updated permissions", ret == 0 && ds.mode == 0640);

    ret = shmctl(id, IPC_INFO, &info);
    check("shmctl.10 IPC_INFO", ret >= 0);
    if (ret >= 0)
        check("shmctl.11 IPC_INFO limits",
              info.max_segments > 0 &&
              info.max_pages_per_segment > 0 &&
              info.max_total_pages ==
                  info.max_segments * info.max_pages_per_segment);

    ret = shmctl(id, SHM_INFO, &info);
    check("shmctl.12 SHM_INFO", ret >= 0);
    if (ret >= 0) {
        check("shmctl.13 active segments", info.used_segments > 0);
        check("shmctl.14 allocated pages", info.total_pages >= 2);
        check("shmctl.15 attachment counts",
              info.total_attachments >= 0 && info.highest_index >= 0);
    }

    check("shmctl.16 SHM_STAT",
          shmctl(id - 1, SHM_STAT, &ds) == id);
    check("shmctl.17 SHM_STAT_ANY",
          shmctl(id - 1, SHM_STAT_ANY, &ds) == id);
    check("shmctl.18 reject invalid table index",
          shmctl(-1, SHM_STAT, &ds) == -1);

    check("shmctl.19 IPC_RMID", shmctl(id, IPC_RMID, 0) == 0);
    check("shmctl.20 removed segment is gone",
          shmctl(id, IPC_STAT, &ds) == -1);

    printf(1, "\nPassed: %d\nFailed: %d\n", passed, failed);
    exit();
}
