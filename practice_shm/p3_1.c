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

    int shmid = shmget(key, 4096, IPC_CREAT | 0666);

    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    int *ptr = (int *)shmat(shmid, NULL, 0);

    if (ptr == (void *)-1) {
        perror("shmat");
        exit(1);
    }

    *ptr = 12345;

    printf("Process A virtual address: %p\n", (void *)ptr);
    printf("Value written: %d\n", *ptr);

    shmdt(ptr);

    return 0;
}