#include "types.h"
#include "user.h"
#include "shm.h"

#define PGSIZE_TEST 4096

int passed = 0;
int failed = 0;
char testname[48];

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

int isbad(char *p) {
    return p == (char *)-1 || p == 0;
}

int main(int argc, char *argv[]) {
    int id, pid, w, ret;
    char *p;
    char *child_argv[] = { "shmexec_test", "child", 0 };
    char *bad_argv[] = { "no_such_exec_program", 0 };

    /* Reached only after the parent successfully execs this program. */
    if (argc > 1 && strcmp(argv[1], "child") == 0) {
        printf(1, "exec.1 successful exec pass\n");
        exit();
    }

    id = shmget(0x590000 + getpid(), PGSIZE_TEST, IPC_CREAT | 0600);
    check("exec.2 create segment", id >= 0);
    if (id < 0)
        exit();

    p = (char *)shmat(id, 0, 0);
    check("exec.3 attach before exec tests", !isbad(p));
    if (isbad(p)) {
        shmctl(id, IPC_RMID, 0);
        exit();
    }

    p[0] = 'P';

    ret = exec("no_such_exec_program", bad_argv);
    check("exec.4 nonexistent program fails", ret == -1);
    check("exec.5 mapping usable after failed exec", p[0] == 'P');

    pid = fork();
    if (pid == 0) {
        exec("shmexec_test", child_argv);
        printf(1, "exec.1 successful exec fail\n");
        exit();
    }

    if (pid < 0) {
        check("exec.6 fork for successful exec", 0);
    } else {
        w = wait();
        check("exec.6 wait for exec child", w == pid);
        check("exec.7 parent mapping survives child exec", p[0] == 'P');
    }

    check("exec.8 parent detach", shmdt(p) == 0);
    check("exec.9 remove segment", shmctl(id, IPC_RMID, 0) == 0);

    printf(1, "\nPassed: %d\nFailed: %d\n", passed, failed);
    exit();
}