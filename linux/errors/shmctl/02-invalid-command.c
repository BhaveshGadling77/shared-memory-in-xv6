#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <string.h>

int main(void) {
	key_t key = 1234;

	int shmid = shmget(key, 4096, IPC_CREAT | IPC_EXCL | 0600);

	if (shmid == -1) {
		perror("shmget");
		return 1;
	}

	printf("Created shmid = %d\n", shmid);
	struct shmid_ds ds;
	errno = 0;
	int ret = shmctl(shmid, 9999, &ds);

	if (ret == -1) {
		printf("[assumption true] Invalid command rejected\n");
		printf("errno = %d\n", errno);
		printf("error = %s\n", strerror(errno));
	} else {
		printf("[assumption false] Invalid command accepted\n");
	}

	shmctl(shmid, IPC_RMID, NULL);

	return 0;
}
