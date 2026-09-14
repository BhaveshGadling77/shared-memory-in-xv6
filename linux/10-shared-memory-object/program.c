#include<stdio.h>
#include<stdlib.h>
#include<sys/shm.h>
#include<sys/wait.h>
#include<unistd.h>
#include<fcntl.h>
#include<string.h>

typedef struct shared_data {
	int status;
	int count;
	int numbers[5];
	char message[100];
} shared_data;


int main() {

	int shmid = shmget(1234, 4096, IPC_CREAT | 0666);

	shared_data sd = {
		.status = 1,
		.count = 5,
		.numbers = {10, 20, 30, 40, 50},
		.message = "Hello From Parent"
	};

	if (shmid == -1) {
		perror("shmget");
		exit(1);
	}

	printf("shared memory ID: %d\n", shmid);

	shared_data *ptr = (shared_data *)shmat(shmid, NULL, 0);
	
	memcpy(ptr, &sd, sizeof(shared_data));

	pid_t pid = fork();

	if (pid == 0) {
		printf("Child: \n");
		printf("the shared data:\nstatus = %d\ncount = %d\nmessage = %s\n"
		 			, ptr->status, ptr->count, ptr->message);
		printf("Numbers: ");
		for (int i = 0; i < 5; i++)
			printf("%d, ", ptr->numbers[i]);
		printf("\n");
		
		ptr->status = 5;
		strcpy(ptr->message, "Modified by child");
		ptr->numbers[0] = 999;
	} else {
		wait(NULL);

		printf("\n");
		printf("Parent: \n");
		printf("the shared data:\nstatus = %d\ncount = %d\nmessage = %s\n"
		 			, ptr->status, ptr->count, ptr->message);
		printf("Numbers: ");
		for (int i = 0; i < 5; i++)
			printf("%d, ", ptr->numbers[i]);
		printf("\n");

		shmdt(ptr);
		shmctl(shmid, IPC_RMID, NULL);
	}
	return 0;

}
