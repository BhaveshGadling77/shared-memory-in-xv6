#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int main(void) {
    key_t key = 12093;

    int shmid = shmget(key, 8192, IPC_CREAT | IPC_EXCL | 0666);

    if (shmid == -1) {
        perror("shmget");
        return 1;
    }

    printf("shmid = %d\n\n", shmid);

    struct shmid_ds ds;

    if (shmctl(shmid, IPC_STAT, &ds) == -1) {
        perror("shmctl(IPC_STAT)");
        shmctl(shmid, IPC_RMID, NULL);
        return 1;
    }

    printf("Shared Memory Info\n");
    printf("shm_perm.uid = %u\n", ds.shm_perm.uid);
    printf("shm_perm.gid = %u\n", ds.shm_perm.gid);
    printf("shm_perm.mode = %o\n", ds.shm_perm.mode);
    printf("shm_segsz = %ld\n", ds.shm_segsz);
    printf("shm_cpid = %d\n", ds.shm_cpid);
    printf("shm_lpid = %d\n", ds.shm_lpid);
    printf("shm_nattch = %ld\n", (long)ds.shm_nattch);
    printf("shm_ctime = %ld\n", (long)ds.shm_ctime);
    printf("shm_atime = %ld\n", (long)ds.shm_atime);
    printf("shm_dtime = %ld\n", (long)ds.shm_dtime);

    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}
