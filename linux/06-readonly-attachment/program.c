#include<stdio.h>
#include<stdlib.h>
#include<sys/shm.h>
#include<sys/wait.h>
#include<string.h>
#include<unistd.h>
#include<fcntl.h>

int main() {

	int shmid = shmget(1234, 4096, IPC_CREAT | 0666);

	if (shmid == -1) {
		perror("shmget");
		exit(1);
	}

	printf("shared memory ID: %d\n", shmid);

	int *ptr = (int *)shmat(shmid, NULL, 0);
	pid_t pid = fork();
	*ptr = 100;
	if (pid == 0) {
		shmdt(ptr);
		ptr = shmat(shmid, NULL, SHM_RDONLY);
		printf("child reads: %d\n", *ptr);

		*ptr = 200; //deliberately try writing
		printf("after modification: %d\n", *ptr);
	} else {
		
		wait(NULL);
		shmdt(ptr);
		shmctl(shmid, IPC_RMID, NULL);
	}
	return 0;
}
