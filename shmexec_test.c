#include "types.h"
#include "user.h"
#include "shm.h"

#define PGSIZE_TEST 4096

struct testcase {
    char testname[16];
    char path[24];
    int expected_result;
} testcases[] = {
    { "shmexec.1", "no_such_program", -1 },
    { "shmexec.2", "README", -1 }
};

int passed = 0;
int failed = 0;
char *p;
char testname[16];

void pass(void) {
    printf(1, "%s pass\n", testname);
    passed++;
}

void fail(void) {
    printf(1, "%s fail\n", testname);
    failed++;
}

void check(char *name, int result) {
    strcpy(testname, name);
    if (result)
        pass();
    else
        fail();
}

int isbad(char *p) {
    return p == (char *)-1 || p == 0;
}

void test(struct testcase t) {
    char *argv[2];
    int ret;

    strcpy(testname, t.testname);
    argv[0] = t.path;
    argv[1] = 0;
    ret = exec(t.path, argv);

    if (ret == t.expected_result)
        pass();
    else
        fail();
}

void test_exec_success(void) {
    int pid, w;
    char *child_argv[] = { "shmexec_test", "child", 0 };

    pid = fork();
    if (pid == 0) {
        exec("shmexec_test", child_argv);
        printf(1, "shmexec.4 fail\n");
        exit();
    }

    if (pid < 0) {
        check("shmexec.4", 0);
        return;
    }

    w = wait();
    check("shmexec.5", w == pid);
    check("shmexec.6", p[0] == 'P');
}

int main(int argc, char *argv[]) {
    int i, id;

    if (argc > 1 && strcmp(argv[1], "child") == 0) {
        strcpy(testname, "shmexec.4");
        pass();
        exit();
    }

    id = shmget(0x590000 + getpid(), PGSIZE_TEST, IPC_CREAT | 0600);
    if (id < 0) {
        strcpy(testname, "shmexec.setup");
        fail();
        exit();
    }

    p = (char *)shmat(id, 0, 0);
    if (isbad(p)) {
        strcpy(testname, "shmexec.setup");
        fail();
        shmctl(id, IPC_RMID, 0);
        exit();
    }
    p[0] = 'P';

    for (i = 0; i < sizeof(testcases) / sizeof(testcases[0]); i++)
        test(testcases[i]);

    check("shmexec.3", p[0] == 'P');

    test_exec_success();

    check("shmexec.7", shmdt(p) == 0);
    check("shmexec.8", shmctl(id, IPC_RMID, 0) == 0);

    printf(1, "\nPassed: %d\nFailed: %d\n", passed, failed);
    exit();
}