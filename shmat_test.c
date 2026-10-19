#include "types.h"
#include "user.h"
#include "shm.h"

#define PGSIZE_TEST 4096

int passed = 0;
int failed = 0;
char testname[40];

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

int main(void) {
    int id, id2;
    char *p, *q;

    id = shmget(0x540000 + getpid(), 2 * PGSIZE_TEST, IPC_CREAT | 0600);
    check("shmat.1 create segment", id >= 0);

    if (id < 0)
        exit();

    p = (char *)shmat(id, 0, 0);
    check("shmat.2 attach segment", !isbad(p));

    if (!isbad(p)) {
        q = (char *)shmat(id, 0, 0);
        check("shmat.3 second attachment", !isbad(q));

        if (!isbad(q)) {
            p[0] = 'A';
            check("shmat.4 shared first page", q[0] == 'A');
            p[PGSIZE_TEST] = 'B';
            check("shmat.5 shared second page", q[PGSIZE_TEST] == 'B');
            check("shmat.6 detach second mapping", shmdt(q) == 0);
        }

        check("shmat.7 invalid ID", shmat(-1, 0, 0) == (char *)-1);
        check("shmat.8 invalid flags",
              shmat(id, 0, 0x40000000) == (char *)-1);
        check("shmat.9 detach first mapping", shmdt(p) == 0);
    }

    id2 = shmget(0x540100 + getpid(), PGSIZE_TEST, IPC_CREAT | 0600);
    check("shmat.10 create remap segment", id2 >= 0);

    if (id2 >= 0) {
        p = (char *)shmat(id, (char *)0x70020000, 0);
        check("shmat.11 attach for remap", !isbad(p));

        if (!isbad(p)) {
            p[0] = 'M';
            q = (char *)shmat(id2, (char *)0x70020000, SHM_REMAP);
            check("shmat.12 SHM_REMAP", !isbad(q));
            if (!isbad(q))
                check("shmat.13 replacement segment data", q[0] == 0);
            if (!isbad(q))
                check("shmat.14 detach remapped mapping", shmdt(q) == 0);
            else
                shmdt(p);
        }

        shmctl(id2, IPC_RMID, 0);
    }

    shmctl(id, IPC_RMID, 0);
    printf(1, "\nPassed: %d\nFailed: %d\n", passed, failed);
    exit();
}
