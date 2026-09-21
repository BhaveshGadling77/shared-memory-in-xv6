#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <string.h>

int main(void) {
	key_t key = 12345;

	int shmid = shmget(key, 0, IPC_CREAT | 0666);

	if (shmid == -1) {
		printf("[test passed] shmget(size=0) failed as expected\n");
		printf("errno  = %d\n", errno);
		//perror("failed because you can't allocate size=0 shared memory \n"); //just curiosity

		//just got to know strerror function exist for the converting errno to string
		printf("Exact Error: %s\n", strerror(errno));
	} else {
		printf("[test failed] shmget(size=0) unexpectedly succeeded\n");
		printf("shmid  = %d\n", shmid);
		shmctl(shmid, IPC_RMID, NULL);
	}

	return 0;
}
