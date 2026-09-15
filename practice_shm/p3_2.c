#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int main()
{
    key_t key = ftok(".", 'E');

    if (key == -1) {
        perror("ftok");
        exit(1);
    }

    int shmid = shmget(key, 4096, 0666);

    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    int *ptr = (int *)shmat(shmid, NULL, SHM_RDONLY);

    if (ptr == (void *)-1) {
        perror("shmat");
        exit(1);
    }

    printf("Process B virtual address: %p\n", (void *)ptr);
    printf("Value read: %d\n", *ptr);

    shmdt(ptr);

    return 0;
}