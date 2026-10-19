#include "types.h"
#include "user.h"
#include "shm.h"

#define CMD_CREATE -100

#define ID_SEG 0
#define ID_BAD 1
#define ID_INDEX 2

#define CHK_NONE 0
#define CHK_SIZE 1
#define CHK_META 2
#define CHK_MODE 3
#define CHK_LIMITS 4
#define CHK_ACTIVE 5
#define CHK_PAGES 6
#define CHK_ATTACH 7
#define CHK_RET_ID 8

struct testcase {
    char testname[16];
    char *description;
    int idkind;
    int cmd;
    int usebuf;
    int check;
    int expected_result;
} testcases[] = {
    { "shmctl.1", "create 2-page segment",
      ID_SEG, CMD_CREATE, 0, CHK_NONE, 0 },
    { "shmctl.2", "IPC_STAT on a valid segment",
      ID_SEG, IPC_STAT, 1, CHK_NONE, 0 },
    { "shmctl.3", "IPC_STAT reports the segment size",
      ID_SEG, IPC_STAT, 1, CHK_SIZE, 0 },
    { "shmctl.4", "IPC_STAT reports shmid, pages, creator pid and removal flag",
      ID_SEG, IPC_STAT, 1, CHK_META, 0 },
    { "shmctl.5", "IPC_STAT with a null buffer must fail",
      ID_SEG, IPC_STAT, 0, CHK_NONE, -1 },
    { "shmctl.6", "IPC_STAT with an invalid shmid must fail",
      ID_BAD, IPC_STAT, 1, CHK_NONE, -1 },
    { "shmctl.7", "invalid command must fail",
      ID_SEG, 999, 1, CHK_NONE, -1 },
    { "shmctl.8", "IPC_SET updates permission bits",
      ID_SEG, IPC_SET, 1, CHK_NONE, 0 },
    { "shmctl.9", "IPC_STAT shows the updated permissions",
      ID_SEG, IPC_STAT, 1, CHK_MODE, 0 },
    { "shmctl.10", "IPC_INFO returns subsystem information",
      ID_SEG, IPC_INFO, 1, CHK_NONE, 0 },
    { "shmctl.11", "IPC_INFO limits are consistent",
      ID_SEG, IPC_INFO, 1, CHK_LIMITS, 0 },
    { "shmctl.12", "SHM_INFO returns subsystem information",
      ID_SEG, SHM_INFO, 1, CHK_NONE, 0 },
    { "shmctl.13", "SHM_INFO reports active segments",
      ID_SEG, SHM_INFO, 1, CHK_ACTIVE, 0 },
    { "shmctl.14", "SHM_INFO reports allocated pages",
      ID_SEG, SHM_INFO, 1, CHK_PAGES, 0 },
    { "shmctl.15", "SHM_INFO attachment count and highest index are valid",
      ID_SEG, SHM_INFO, 1, CHK_ATTACH, 0 },
    { "shmctl.16", "SHM_STAT returns the shmid for a table index",
      ID_INDEX, SHM_STAT, 1, CHK_RET_ID, 0 },
    { "shmctl.17", "SHM_STAT_ANY returns the shmid for a table index",
      ID_INDEX, SHM_STAT_ANY, 1, CHK_RET_ID, 0 },
    { "shmctl.18", "SHM_STAT with an invalid table index must fail",
      ID_BAD, SHM_STAT, 1, CHK_NONE, -1 },
    { "shmctl.19", "IPC_RMID removes the segment",
      ID_SEG, IPC_RMID, 0, CHK_NONE, 0 },
    { "shmctl.20", "IPC_STAT on a removed segment must fail",
      ID_SEG, IPC_STAT, 1, CHK_NONE, -1 }
};

union ctlbuf {
    struct shmctl_ds ds;
    struct shmctl_info info;
} buf;

int passed = 0;
int failed = 0;
int segid = -1;
char testname[16];
char *testdesc;

void pass(void) {
    printf(1, "%s pass -> %s\n", testname, testdesc);
    passed++;
}

void fail(void) {
    printf(1, "%s fail -> %s\n", testname, testdesc);
    failed++;
}

int verify(int kind, int ret) {
    switch (kind) {
    case CHK_SIZE:
        return buf.ds.size == 8192;
    case CHK_META:
        return buf.ds.shmid == segid && buf.ds.npages == 2 &&
               buf.ds.creator_pid == getpid() &&
               buf.ds.marked_for_removal == 0;
    case CHK_MODE:
        return buf.ds.mode == 0640;
    case CHK_LIMITS:
        return buf.info.max_segments > 0 &&
               buf.info.max_pages_per_segment > 0 &&
               buf.info.max_total_pages ==
                   buf.info.max_segments * buf.info.max_pages_per_segment;
    case CHK_ACTIVE:
        return buf.info.used_segments > 0;
    case CHK_PAGES:
        return buf.info.total_pages >= 2;
    case CHK_ATTACH:
        return buf.info.total_attachments >= 0 && buf.info.highest_index >= 0;
    case CHK_RET_ID:
        return ret == segid;
    }
    return 1;
}

void test(struct testcase t) {
    int id, ret, result;

    strcpy(testname, t.testname);
    testdesc = t.description;

    if (t.cmd == CMD_CREATE) {
        segid = shmget(0x560000 + getpid(), 8192, IPC_CREAT | 0600);
        result = segid >= 0 ? 0 : -1;
    } else {
        if (t.idkind == ID_SEG)
            id = segid;
        else if (t.idkind == ID_INDEX)
            id = segid - 1;
        else
            id = -1;

        if (t.cmd == IPC_SET)
            buf.ds.mode = 0640;

        ret = shmctl(id, t.cmd, t.usebuf ? (void *)&buf : 0);
        result = ret < 0 ? -1 : 0;
        if (result == 0 && !verify(t.check, ret))
            result = -1;
    }

    if (result == t.expected_result)
        pass();
    else
        fail();
}

int main(int argc, char *argv[]) {
    int i;

    for (i = 0; i < sizeof(testcases) / sizeof(testcases[0]); i++)
        test(testcases[i]);

    printf(1, "\nPassed: %d\nFailed: %d\n", passed, failed);
    exit();
}