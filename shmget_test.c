#include "types.h"
#include "user.h"
#include "shm.h"

#define PGSIZE_TEST 4096
//this is according to sir's template.
struct testcase {
    char testname[16];
    char *description;
    int keyoff;
    int size;
    int flags;
    int expected_result;
} testcases[] = {
    { "shmget.1", "create new segment with a different key",
      1, PGSIZE_TEST, IPC_CREAT | 0600, 0 },
    { "shmget.2", "get existing key without IPC_CREAT",
      0, PGSIZE_TEST, 0, 0 },
    { "shmget.3", "IPC_CREAT | IPC_EXCL on existing key must fail",
      0, PGSIZE_TEST, IPC_CREAT | IPC_EXCL | 0600, -1 },
    { "shmget.4", "nonexistent key without IPC_CREAT must fail",
      2, PGSIZE_TEST, 0, -1 },
    { "shmget.5", "create segment with size 0 must fail",
      3, 0, IPC_CREAT | 0600, -1 },
    { "shmget.6", "negative size must fail",
      4, -1, IPC_CREAT | 0600, -1 },
    { "shmget.7", "size above maximum segment size must fail",
      5, 17 * PGSIZE_TEST, IPC_CREAT | 0600, -1 }
};

int passed = 0;
int failed = 0;
int basekey;
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

void test(struct testcase t) {
    int id, result;

    strcpy(testname, t.testname);
    testdesc = t.description;
    id = shmget(basekey + t.keyoff, t.size, t.flags);
    result = id >= 0 ? 0 : -1;

    if (result == t.expected_result)
        pass();
    else
        fail();

    if (id >= 0 && (t.flags & IPC_CREAT) && t.keyoff != 0)
        shmctl(id, IPC_RMID, 0);
}

int main(int argc, char *argv[]) {
    int i, id;

    basekey = 0x530000 + getpid();
    id = shmget(basekey, PGSIZE_TEST, IPC_CREAT | 0600);
    if (id < 0) {
        strcpy(testname, "shmget.setup");
        testdesc = "create base segment";
        fail();
        exit();
    }

    for (i = 0; i < sizeof(testcases) / sizeof(testcases[0]); i++)
        test(testcases[i]);

    shmctl(id, IPC_RMID, 0);
    printf(1, "\nPassed: %d\nFailed: %d\n", passed, failed);
    exit();
}