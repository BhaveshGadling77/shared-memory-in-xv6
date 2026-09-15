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

    // Create shared memory

    shmid = shmget(key, SHM_SIZE, IPC_CREAT | 0666);

    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    chat = (struct Chat *)shmat(shmid, NULL, 0);

    if (chat == (void *)-1) {
        perror("shmat");
        exit(1);
    }

    // Initialize shared memory

    chat->turn = 2;
    chat->exit_chat = 0;

    printf("\n========== CHAT 1 ==========\n");
    printf("Waiting for Chat 2...\n\n");

    while (1){

        //Wait for Chat 2's turn

        while (chat->turn != 1 && chat->exit_chat == 0) {
            usleep(100000);
        }

        if (chat->exit_chat)
            break;

        //Display Chat 2's message

        if (chat->sender == 2) {
            printf("\n[%s] Chat 2: %s\n", chat->time, chat->message);
        }

        // Take Chat 1's message

        printf("Chat 1: ");

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

        strftime(chat->time, sizeof(chat->time), "%H:%M:%S", t);

        // Identify sender

        chat->sender = 1;

        //Give turn to Chat 2

        chat->turn = 2;
    }

    printf("\nChat ended.\n");

    shmdt(chat);

    //Chat 1 removes the shared memory

    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}