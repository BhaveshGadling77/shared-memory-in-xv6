#include "types.h"
#include "user.h"
#include "shm.h"

int passed = 0;
int failed = 0;
char testname[40];

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
    int id;
    char *p, *q;

    id = shmget(0x550000 + getpid(), 4096, IPC_CREAT | 0600);
    check("shmdt.1 create segment", id >= 0);

    if (id >= 0) {
        p = (char *)shmat(id, 0, 0);
        check("shmdt.2 attach segment", !isbad(p));

        if (!isbad(p)) {
            check("shmdt.3 reject wrong base address", shmdt(p + 1) == -1);
            check("shmdt.4 detach mapping", shmdt(p) == 0);
            check("shmdt.5 reject detached mapping", shmdt(p) == -1);
        }

        q = (char *)shmat(id, 0, 0);
        check("shmdt.6 attach again", !isbad(q));
        if (!isbad(q))
            check("shmdt.7 detach again", shmdt(q) == 0);

        check("shmdt.8 reject null address", shmdt(0) == -1);
        shmctl(id, IPC_RMID, 0);
    }

    printf(1, "\nPassed: %d\nFailed: %d\n", passed, failed);
    exit();
}
