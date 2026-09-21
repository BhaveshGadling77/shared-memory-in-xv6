#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <string.h>

int main(void) {
	/*
	 * this key should not correspond to an existing
	 * shared-memory segment. as i have used a random key
	 */
	key_t key = 202029;

	int shmid = shmget(key, 4096, 0666);

	if (shmid == -1) {
		printf("[assumption true] shmget(nonexistent key) failed as expected\n");
		printf("errno Number = %d\n", errno);
		printf("error  = %s\n", strerror(errno));
	} else {
		printf("[assumption failed] shmget unexpectedly succeeded\n");
		printf("shmid  = %d\n", shmid);
	}

	return 0;
}
