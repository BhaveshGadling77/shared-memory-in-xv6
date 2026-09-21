#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <string.h>

int main(void) {
	key_t key = 1234;
	int shmid1 = shmget(key, 4096, IPC_CREAT);

	if (shmid1 == -1) {
		perror("Initial shmget");
		return 1;
	}

	printf("Created segment: shmid = %d\n", shmid1);

	int shmid2 = shmget(key, 4096, IPC_CREAT | IPC_EXCL | 0666); //it should fail as per my assumptions

	if (shmid2 == -1) {
		printf("[assumption true] IPC_EXCL failed as expected\n");
		printf("errno: %d\n", errno);
		printf("error: %s\n", strerror(errno));
	} else {
		printf("[assumption false] IPC_EXCL unexpectedly succeeded\n");
		printf("shmid  = %d\n", shmid2);
		shmctl(shmid2, IPC_RMID, NULL);
	}
	//cleanup
	shmctl(shmid1, IPC_RMID, NULL);

	return 0;
}
