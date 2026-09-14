#include<stdio.h>
#include<stdlib.h>
#include<sys/shm.h>
#include<unistd.h>
#include<fcntl.h>

int main() {

	int shmid = shmget(1234, 4096, IPC_CREAT | 0666);

	if (shmid == -1) {
		perror("shmget");
		exit(1);
	}

	printf("shared memory ID: %d\n", shmid);

	int *ptr1 = (int *)shmat(shmid, NULL, 0);
	int *ptr2 = (int *)shmat(shmid, NULL, 0);

	printf("ptr1: %p, ptr2: %p\n", ptr1, ptr2);

	*ptr1 = 100;
	
	printf("ptr2 content: %d\n", *ptr2); //read from ptr2
	
	shmdt(ptr1);

	printf("again ptr2 contents: %d\n", *ptr2);

	shmdt(ptr2);

	shmctl(shmid, IPC_RMID, NULL);

}
