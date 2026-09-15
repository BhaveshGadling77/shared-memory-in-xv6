#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int main()
{
    key_t key = ftok(".", 'I');

    size_t requested_size = 100;

    int shmid = shmget(key,
                       requested_size,
                       IPC_CREAT | 0666);

    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    struct shmid_ds info;

    if (shmctl(shmid, IPC_STAT, &info) == -1) {
        perror("shmctl IPC_STAT");
        exit(1);
    }

    printf("Requested size: %zu bytes\n",
           requested_size);

    printf("Segment size reported by kernel: %zu bytes\n",
           (size_t)info.shm_segsz);

    printf("Shared Memory ID: %d\n", shmid);

    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}