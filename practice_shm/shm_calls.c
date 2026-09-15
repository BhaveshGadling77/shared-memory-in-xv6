#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int main()
{
    key_t key;
    int shmid;
    char *data;

    // Step 1: Generate a key
    key = ftok(".", 'A');

    if (key == -1) {
        perror("ftok");
        exit(1);
    }

    printf("Generated key: %d\n", key);

    // Step 2: Create/get shared memory
    shmid = shmget(key, 4096, IPC_CREAT | 0666);

    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    printf("Shared Memory ID: %d\n", shmid);

    // Step 3: Attach shared memory
    // here when we have null we tell kerenl to select any availiable virtual space in process virtual space and map the shared memory segment out thier like 
    data = (char *)shmat(shmid, NULL, 0);

    if (data == (char *)-1) {
        perror("shmat");
        exit(1);
    }

    printf("Attached at virtual address: %p\n", (void *)data);

    // Step 4: Write
    strcpy(data, "Hello Shared Memory!");

    // Step 5: Read
    printf("Data read: %s\n", data);

    // Step 6: Detach
    if (shmdt(data) == -1) {
        perror("shmdt");
        exit(1);
    }

    printf("Detached successfully\n");

    // Step 7: Remove segment
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        exit(1);
    }

    printf("Shared memory removed successfully\n");

    return 0;
}