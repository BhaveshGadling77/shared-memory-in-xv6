#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <unistd.h>

#define KEY 1234

struct chat {
    char messageA[256];
    char messageB[256];

    int a_ready;
    int b_ready;
};

int main() {

    int shmid = shmget(KEY, sizeof(struct chat), IPC_CREAT | 0666);

    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    struct chat *chat = (struct chat *)shmat(shmid, NULL, 0);

    if (chat == (void *)-1) {
        perror("shmat");
        exit(1);
    }

    chat->a_ready = 0;
    chat->b_ready = 0;

    printf("processA started\n");

    while (1) {

        printf("processA: ");
        char *ptr = fgets(chat->messageA, sizeof(chat->messageA), stdin);
		if (ptr == NULL) {
			printf("thank you for chatting.\n");
			break;
		}
		

        chat->messageA[strcspn(chat->messageA, "\n")] = '\0';

        if (strcmp(chat->messageA, "exit") == 0)
            break;

        /* Tell processB that a message is ready */
        chat->a_ready = 1;

        /* Busy wait for processB's reply */
        while (chat->b_ready == 0)
            ;

        printf("processB: %s\n", chat->messageB);

        /* Message has been consumed */
        chat->b_ready = 0;
    }

    shmdt(chat);

    /* Remove shared memory */
    shmctl(shmid, IPC_RMID, NULL);

    printf("processA terminated\n");

    return 0;
}
