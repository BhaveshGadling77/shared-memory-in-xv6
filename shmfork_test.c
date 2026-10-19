#include "types.h"
#include "user.h"
#include "shm.h"

#define PGSIZE_TEST 4096
#define CHILDREN 3

#define OP_CREATE 0
#define OP_ATTACH 1
#define OP_FORK_WRITE 2
#define OP_CHECK 3
#define OP_FORK_MANY 4
#define OP_WAIT_ONE 5
#define OP_CHECK_MANY 6
#define OP_NESTED 7
#define OP_CHILD_DETACH 8
#define OP_WRITE_CHECK 9
#define OP_DETACH 10
#define OP_RMID 11

struct testcase {
    char testname[16];
    char *description;
    int op;
    int offset;
    char value;
    int expected_result;
} testcases[] = {
    { "shmfork.1", "create segment", OP_CREATE, 0, 0, 0 },
    { "shmfork.2", "attach segment before fork", OP_ATTACH, 0, 0, 0 },
    { "shmfork.3", "wait for child that writes to the segment",
      OP_FORK_WRITE, 0, 'C', 0 },
    { "shmfork.4", "child write is visible to the parent",
      OP_CHECK, 0, 'C', 0 },
    { "shmfork.5", "create multiple children", OP_FORK_MANY, 0, 0, 0 },
    { "shmfork.6", "wait for first child", OP_WAIT_ONE, 0, 0, 0 },
    { "shmfork.6", "wait for second child", OP_WAIT_ONE, 0, 0, 0 },
    { "shmfork.6", "wait for third child", OP_WAIT_ONE, 0, 0, 0 },
    { "shmfork.7", "writes from multiple children are visible",
      OP_CHECK_MANY, 0, 0, 0 },
    { "shmfork.8", "wait for nested fork child", OP_NESTED, 32, 'N', 0 },
    { "shmfork.9", "nested grandchild write is visible",
      OP_CHECK, 32, 'N', 0 },
    { "shmfork.10", "wait for child that detaches the segment",
      OP_CHILD_DETACH, 0, 0, 0 },
    { "shmfork.11", "parent mapping remains usable after child detach",
      OP_WRITE_CHECK, 48, 'R', 0 },
    { "shmfork.12", "parent detaches the segment", OP_DETACH, 0, 0, 0 },
    { "shmfork.13", "remove the segment", OP_RMID, 0, 0, 0 }
};

int passed = 0;
int failed = 0;
int id = -1;
int nchildren;
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
    int pid, gpid, i;
    int result = -1;

    strcpy(testname, t.testname);
    testdesc = t.description;

    switch (t.op) {
    case OP_CREATE:
        id = shmget(0x580000 + getpid(), PGSIZE_TEST, IPC_CREAT | 0600);
        result = id >= 0 ? 0 : -1;
        break;
    case OP_ATTACH:
        p = (char *)shmat(id, 0, 0);
        result = isbad(p) ? -1 : 0;
        if (!isbad(p))
            p[0] = 'P';
        break;
    case OP_FORK_WRITE:
        pid = fork();
        if (pid == 0) {
            if (!isbad(p))
                p[t.offset] = t.value;
            exit();
        }
        if (pid > 0 && wait() == pid)
            result = 0;
        break;
    case OP_CHECK:
        if (!isbad(p))
            result = p[t.offset] == t.value ? 0 : -1;
        break;
    case OP_FORK_MANY:
        for (i = 0; i < CHILDREN; i++) {
            pid = fork();
            if (pid == 0) {
                if (!isbad(p))
                    p[16 + i] = 'a' + i;
                exit();
            }
            if (pid < 0)
                break;
        }
        nchildren = i;
        result = i == CHILDREN ? 0 : -1;
        break;
    case OP_WAIT_ONE:
        result = wait() > 0 ? 0 : -1;
        break;
    case OP_CHECK_MANY:
        if (!isbad(p) && nchildren == CHILDREN &&
            p[16] == 'a' && p[17] == 'b' && p[18] == 'c')
            result = 0;
        break;
    case OP_NESTED:
        pid = fork();
        if (pid == 0) {
            gpid = fork();
            if (gpid == 0) {
                if (!isbad(p))
                    p[t.offset] = t.value;
                exit();
            }
            if (gpid > 0)
                wait();
            exit();
        }
        if (pid > 0 && wait() == pid)
            result = 0;
        break;
    case OP_CHILD_DETACH:
        pid = fork();
        if (pid == 0) {
            shmdt(p);
            exit();
        }
        if (pid > 0 && wait() == pid)
            result = 0;
        break;
    case OP_WRITE_CHECK:
        if (!isbad(p)) {
            p[t.offset] = t.value;
            result = p[t.offset] == t.value ? 0 : -1;
        }
        break;
    case OP_DETACH:
        result = shmdt(p) == 0 ? 0 : -1;
        break;
    case OP_RMID:
        result = shmctl(id, IPC_RMID, 0) == 0 ? 0 : -1;
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

    printf(1, "\nPassed: %d\nFailed: %d\n", passed, failed);
    exit();
}