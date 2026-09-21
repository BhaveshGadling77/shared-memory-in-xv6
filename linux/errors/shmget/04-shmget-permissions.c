#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

int main(void) {
	key_t key = 1234;

	int shmid = shmget(key, 4096, IPC_CREAT | 0600);

	if (shmid == -1) {
		perror("Initial shmget");
		return 1;
	}

	printf("Created segment: shmid = %d\n", shmid);

	struct shmid_ds ds;

	if (shmctl(shmid, IPC_STAT, &ds) == -1) {
		perror("IPC_STAT");
		shmctl(shmid, IPC_RMID, NULL);
		return 1;
	}

	ds.shm_perm.mode = 0000;

	if (shmctl(shmid, IPC_SET, &ds) == -1) {
		perror("IPC_SET");
		shmctl(shmid, IPC_RMID, NULL);
		return 1;
	}

	printf("Changed permissions to 000\n");

	errno = 0;
	int result = shmget(key, 4096, 0666);
	if (result == -1) {
		printf("shmget failed\n");
		printf("errno: %d\n", errno);
		printf("error: %s\n", strerror(errno));
	} else {
		printf("shmget succeeded: shmid = %d\n", result);
	}

	shmctl(shmid, IPC_RMID, NULL);
	return 0;
}
