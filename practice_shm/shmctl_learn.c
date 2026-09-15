#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int main()
{
    key_t key = ftok(".", 'G');

    int shmid = shmget(key, 4096,
                       IPC_CREAT | 0666);

    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    char *ptr = shmat(shmid, NULL, 0);

    if (ptr == (void *)-1) {
        perror("shmat");
        exit(1);
    }

    strcpy(ptr, "I am still attached!");

    printf("Before IPC_RMID: %s\n", ptr);

    // Mark for removal
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        exit(1);
    }

    printf("Segment marked for removal.\n");

    // Existing attachment still works
    printf("After IPC_RMID: %s\n", ptr);

    shmdt(ptr);

    printf("Detached.\n");

    return 0;
}