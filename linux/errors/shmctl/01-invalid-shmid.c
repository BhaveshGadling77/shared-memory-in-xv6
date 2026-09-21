#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <string.h>

int main(void) {
	struct shmid_ds ds;

	int invalid_ids[] = {-1, 999999999};

	int n = 2;

	for (int i = 0; i < n; i++) {
		errno = 0;
		int ret = shmctl(invalid_ids[i], IPC_STAT, &ds);

		if (ret == -1) {
			printf("[assumption true] shmid=%d failed\n", invalid_ids[i]);
			printf("errno: %d\n", errno);
			printf("error: %s\n\n", strerror(errno));
		} else {
			printf("[assumption false] shmid=%d unexpectedly succeeded\n\n", invalid_ids[i]);
		}
	}

	return 0;
}
