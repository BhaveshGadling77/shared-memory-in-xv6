#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define SHM_SIZE 1024

struct Chat {
    int sender;
    int turn;
    int exit_chat;
    char message[512];
    char time[20];
};

int main(){
    key_t key;
    int shmid;
    struct Chat *chat;

    key = ftok(".", 'C');

    if (key == -1) {
        perror("ftok");
        exit(1);
    }

    // Get the existing shared memory

    shmid = shmget(key, SHM_SIZE, 0666);

    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    chat = (struct Chat *)shmat(shmid, NULL, 0);

    if (chat == (void *)-1) {
        perror("shmat");
        exit(1);
    }

    printf("\n========== CHAT 2 ==========\n");

    printf("Connected to Chat 1...\n\n");

    while (1){
        //Wait for Chat 2's turn
        while (chat->turn != 2 && chat->exit_chat == 0) {
            usleep(100000);
        }

        if (chat->exit_chat)
            break;
        //Display Chat 1's essage

        if (chat->sender == 1) {
            printf("\n[%s] Chat 1: %s\n",
                   chat->time,
                   chat->message);
        }

        // Take Chat 2's message

        printf("Chat 2: ");
        fgets(chat->message, sizeof(chat->message), stdin);
        chat->message[strcspn(chat->message, "\n")] = '\0';

        //Check for exit

        if (strcmp(chat->message, "exit") == 0){
            chat->exit_chat = 1;
            break;
        }
        //get the current time
        time_t now = time(NULL);
        struct tm *t = localtime(&now);

        strftime(chat->time,sizeof(chat->time),"%H:%M:%S",t);

        // Identify sender
        chat->sender = 2;
        //Give turn to Chat 1
        chat->turn = 1;

    }

    printf("\nChat ended.\n");

    shmdt(chat);

    return 0;

}