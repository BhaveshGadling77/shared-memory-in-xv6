#include "types.h"
#include "user.h"
#include "shm.h"

#define PGSIZE_TEST 4096
//this is just for testing purposes
#define REMAP_ADDR 0x70020000
//this macros are for testing purpose
#define OP_CREATE 0
#define OP_CREATE2 1
#define OP_ATTACH_P 2
#define OP_ATTACH_Q 3
#define OP_ATTACH_RAW 4
#define OP_SHARE 5
#define OP_DETACH_P 6
#define OP_DETACH_Q 7
#define OP_REMAP 8
#define OP_ZERO 9

struct testcase {
    char testname[16];
    char *description;
    int op;
    int valid_id;
    uint addr;
    int flags;
    int offset;
    char value;
    int expected_result;
} testcases[] = {
    { "shmat.1", "create 2-page segment",
      OP_CREATE, 0, 0, 0, 0, 0, 0 },
    { "shmat.2", "attach segment at kernel-chosen address",
      OP_ATTACH_P, 0, 0, 0, 0, 0, 0 },
    { "shmat.3", "second attachment of the same segment",
      OP_ATTACH_Q, 0, 0, 0, 0, 0, 0 },
    { "shmat.4", "write to first page is visible through second mapping",
      OP_SHARE, 0, 0, 0, 0, 'A', 0 },
    { "shmat.5", "write to second page is visible through second mapping",
      OP_SHARE, 0, 0, 0, PGSIZE_TEST, 'B', 0 },
    { "shmat.6", "detach second mapping",
      OP_DETACH_Q, 0, 0, 0, 0, 0, 0 },
    { "shmat.7", "invalid shmid must fail",
      OP_ATTACH_RAW, 0, 0, 0, 0, 0, -1 },
    { "shmat.8", "invalid flags must fail",
      OP_ATTACH_RAW, 1, 0, 0x40000000, 0, 0, -1 },
    { "shmat.9", "detach first mapping",
      OP_DETACH_P, 0, 0, 0, 0, 0, 0 },
    { "shmat.10", "create 1-page segment for remap",
      OP_CREATE2, 0, 0, 0, 0, 0, 0 },
    { "shmat.11", "attach 2-page segment at fixed address",
      OP_ATTACH_P, 0, REMAP_ADDR, 0, 0, 0, 0 },
    { "shmat.12", "SHM_REMAP replaces the existing mapping",
      OP_REMAP, 0, REMAP_ADDR, SHM_REMAP, 0, 0, 0 },
    { "shmat.13", "remapped segment starts zeroed",
      OP_ZERO, 0, 0, 0, 0, 0, 0 },
    { "shmat.14", "detach remapped mapping",
      OP_DETACH_Q, 0, 0, 0, 0, 0, 0 }
};

int passed = 0;
int failed = 0;
int segid = -1;
int id2 = -1;
char *p;
char *q;
char testname[16];
char *testdesc;

void pass(void) {
    printf(1, "%s pass - %s\n", testname, testdesc);
    passed++;
}

void fail(void) {
    printf(1, "%s fail - %s\n", testname, testdesc);
    failed++;
}

int isbad(char *s) {
    return s == (char *)-1 || s == 0;
}

void test(struct testcase t) {
    char *r;
    int result = -1;

    strcpy(testname, t.testname);
    testdesc = t.description;

    switch (t.op) {
    case OP_CREATE:
        segid = shmget(0x540000 + getpid(), 2 * PGSIZE_TEST, IPC_CREAT | 0600);
        result = segid >= 0 ? 0 : -1;
        break;
    case OP_CREATE2:
        id2 = shmget(0x540100 + getpid(), PGSIZE_TEST, IPC_CREAT | 0600);
        result = id2 >= 0 ? 0 : -1;
        break;
    case OP_ATTACH_P:
        p = (char *)shmat(segid, (void *)t.addr, t.flags);
        result = isbad(p) ? -1 : 0;
        break;
    case OP_ATTACH_Q:
        q = (char *)shmat(segid, (void *)t.addr, t.flags);
        result = isbad(q) ? -1 : 0;
        break;
    case OP_ATTACH_RAW:
        r = (char *)shmat(t.valid_id ? segid : -1, (void *)t.addr, t.flags);
        result = isbad(r) ? -1 : 0;
        if (!isbad(r))
            shmdt(r);
        break;
    case OP_SHARE:
        if (!isbad(p) && !isbad(q)) {
            p[t.offset] = t.value;
            result = q[t.offset] == t.value ? 0 : -1;
        }
        break;
    case OP_DETACH_P:
        if (!isbad(p)) {
            result = shmdt(p);
            if (result == 0)
                p = 0;
        }
        break;
    case OP_DETACH_Q:
        if (!isbad(q)) {
            result = shmdt(q);
            if (result == 0)
                q = 0;
        }
        break;
    case OP_REMAP:
        if (!isbad(p))
            p[0] = 'M';
        q = (char *)shmat(id2, (void *)t.addr, t.flags);
        result = isbad(q) ? -1 : 0;
        break;
    case OP_ZERO:
        if (!isbad(q))
            result = q[0] == 0 ? 0 : -1;
        break;
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

    if (!isbad(p))
        shmdt(p);
    if (!isbad(q))
        shmdt(q);
    if (id2 >= 0)
        shmctl(id2, IPC_RMID, 0);
    if (segid >= 0)
        shmctl(segid, IPC_RMID, 0);

    printf(1, "\nPassed: %d\nFailed: %d\n", passed, failed);
    exit();
}