#include "types.h"
#include "user.h"
#include "shm.h"

#define PGSIZE_TEST 4096
#define CHILDREN 3

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

int main(void) {
    int id, pid, w, i, j;
    char *p;
    int childpids[CHILDREN];

    id = shmget(0x580000 + getpid(), PGSIZE_TEST, IPC_CREAT | 0600);
    check("fork.1 create segment", id >= 0);
    if (id < 0)
        exit();

    p = (char *)shmat(id, 0, 0);
    check("fork.2 attach before fork", !isbad(p));
    if (isbad(p)) {
        shmctl(id, IPC_RMID, 0);
        exit();
    }

    p[0] = 'P';
    pid = fork();
    if (pid == 0) {
        p[0] = 'C';
        exit();
    }
    if (pid < 0) {
        check("fork.3 fork succeeds", 0);
    } else {
        w = wait();
        check("fork.3 wait for child", w == pid);
        check("fork.4 child write visible to parent", p[0] == 'C');
    }

    for (i = 0; i < CHILDREN; i++) {
        childpids[i] = fork();
        if (childpids[i] == 0) {
            p[16 + i] = 'a' + i;
            exit();
        }
        if (childpids[i] < 0)
            break;
    }

    check("fork.5 create multiple children", i == CHILDREN);
    for (j = 0; j < i; j++) {
        w = wait();
        check("fork.6 wait for multiple child", w > 0);
    }
    check("fork.7 multiple child writes visible",
          i == CHILDREN && p[16] == 'a' && p[17] == 'b' && p[18] == 'c');

    pid = fork();
    if (pid == 0) {
        int gpid = fork();
        if (gpid == 0) {
            p[32] = 'N';
            exit();
        }
        if (gpid > 0)
            wait();
        exit();
    }
    if (pid < 0) {
        check("fork.8 nested fork created", 0);
    } else {
        w = wait();
        check("fork.8 nested fork child exits", w == pid);
        check("fork.9 nested grandchild write visible", p[32] == 'N');
    }

    pid = fork();
    if (pid == 0) {
        shmdt(p);
        exit();
    }
    if (pid < 0) {
        check("fork.10 child detach fork succeeds", 0);
    } else {
        w = wait();
        check("fork.10 child detach child exits", w == pid);
        p[48] = 'R';
        check("fork.11 parent mapping remains usable", p[48] == 'R');
    }

    check("fork.12 parent detach", shmdt(p) == 0);
    check("fork.13 remove segment", shmctl(id, IPC_RMID, 0) == 0);

    printf(1, "\nPassed: %d\nFailed: %d\n", passed, failed);
    exit();
}
