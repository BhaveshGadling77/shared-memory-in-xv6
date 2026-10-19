#include "types.h"
#include "user.h"
#include "shm.h"

struct testcase {
    char testname[32];
    int key;
    int size;
    int flags;
    int expected_success;
};

int passed = 0;
int failed = 0;
char testname[32];

void pass(void) {
    printf(1, "%s pass\n", testname);
    passed++;
}

void fail(void) {
    printf(1, "%s fail\n", testname);
    failed++;
}

void test(struct testcase t) {
    int id;

    strcpy(testname, t.testname);
    id = shmget(t.key, t.size, t.flags);

    if ((id >= 0) == t.expected_success)
        pass();
    else
        fail();

    if (id >= 0 && (t.flags & IPC_CREAT))
        shmctl(id, IPC_RMID, 0);
}

int main(void) {
    int key;
    int id;
    int i;
    struct testcase testcases[7];

    key = 0x530000 + getpid();
    id = shmget(key, 4096, IPC_CREAT | 0600);

    testcases[0] = (struct testcase){"shmget.1", key + 1, 4096, IPC_CREAT | 0600, 1};
    testcases[1] = (struct testcase){"shmget.2", key, 4096, 0, 1};
    testcases[2] = (struct testcase){"shmget.3", key, 4096, IPC_CREAT | IPC_EXCL | 0600, 0};
    testcases[3] = (struct testcase){"shmget.4", key + 2, 4096, 0, 0};
    testcases[4] = (struct testcase){"shmget.5", key + 3, 0, IPC_CREAT | 0600, 0};
    testcases[5] = (struct testcase){"shmget.6", key + 4, -1, IPC_CREAT | 0600, 0};
    testcases[6] = (struct testcase){"shmget.7", key + 5, 17 * 4096, IPC_CREAT | 0600, 0};

    for (i = 0; i < sizeof(testcases) / sizeof(testcases[0]); i++)
        test(testcases[i]);

    if (id >= 0)
        shmctl(id, IPC_RMID, 0);

    printf(1, "\nPassed: %d\nFailed: %d\n", passed, failed);
    exit();
}
