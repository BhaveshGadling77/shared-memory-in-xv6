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

    int shmid = shmget(KEY, sizeof(struct chat), 0666);

    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    struct chat *chat = (struct chat *)shmat(shmid, NULL, 0);

    if (chat == (void *)-1) {
        perror("shmat");
        exit(1);
    }

    printf("processB started\n");

    while (1) {

        /* Busy wait for processA */
        while (chat->a_ready == 0)
            ;

        printf("processA: %s\n", chat->messageA);

        chat->a_ready = 0;

        if (strcmp(chat->messageA, "exit") == 0)
            break;

        printf("processB: ");
        char *ptr = fgets(chat->messageB, sizeof(chat->messageB), stdin);
		if (ptr == NULL) {
			printf("thank you for chatting\n");
			break;
		}
        chat->messageB[strcspn(chat->messageB, "\n")] = '\0';

        if (strcmp(chat->messageB, "exit") == 0) {
            chat->b_ready = 1;
            break;
        }

        chat->b_ready = 1; //reply ready
    }

    shmdt(chat);

    printf("processB terminated\n");

    return 0;
}
