#include "types.h"
#include "user.h"
#include "shm.h"

#define OP_CREATE 0
#define OP_ATTACH 1
#define OP_DETACH 2
#define OP_DETACH_NULL 3

struct testcase {
    char testname[16];
    char *description;
    int op;
    int offset;
    int expected_result;
} testcases[] = {
    { "shmdt.1", "create segment", OP_CREATE, 0, 0 },
    { "shmdt.2", "attach segment", OP_ATTACH, 0, 0 },
    { "shmdt.3", "reject wrong base address", OP_DETACH, 1, -1 },
    { "shmdt.4", "detach mapping at its base address", OP_DETACH, 0, 0 },
    { "shmdt.5", "reject already detached mapping", OP_DETACH, 0, -1 },
    { "shmdt.6", "attach segment again", OP_ATTACH, 0, 0 },
    { "shmdt.7", "detach segment again", OP_DETACH, 0, 0 },
    { "shmdt.8", "reject null address", OP_DETACH_NULL, 0, -1 }
};

int passed = 0;
int failed = 0;
int segid = -1;
char *p;
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
    int result = -1;

    strcpy(testname, t.testname);
    testdesc = t.description;

    switch (t.op) {
    case OP_CREATE:
        segid = shmget(0x550000 + getpid(), 4096, IPC_CREAT | 0600);
        result = segid >= 0 ? 0 : -1;
        break;
    case OP_ATTACH:
        p = (char *)shmat(segid, 0, 0);
        result = isbad(p) ? -1 : 0;
        break;
    case OP_DETACH:
        if (!isbad(p))
            result = shmdt(p + t.offset);
        break;
    case OP_DETACH_NULL:
        result = shmdt(0);
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

    if (segid >= 0)
        shmctl(segid, IPC_RMID, 0);

    printf(1, "\nPassed: %d\nFailed: %d\n", passed, failed);
    exit();
}