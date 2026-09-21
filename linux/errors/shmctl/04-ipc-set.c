#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int main(void) {
	key_t key = 0x33333;

	int shmid = shmget(key, 4096, IPC_CREAT | IPC_EXCL | 0600);

	if (shmid == -1) {
		perror("shmget");
		return 1;
	}

	struct shmid_ds ds;

	if (shmctl(shmid, IPC_STAT, &ds) == -1) {
		perror("IPC_STAT");
		shmctl(shmid, IPC_RMID, NULL);
		return 1;
	}

	printf("Original mode: %o\n",
		   ds.shm_perm.mode);

	ds.shm_perm.mode = 0666;
	if (shmctl(shmid, IPC_SET, &ds) == -1) {
		perror("IPC_SET");
		shmctl(shmid, IPC_RMID, NULL);
		return 1;
	}
	if (shmctl(shmid, IPC_STAT, &ds) == -1) {
		perror("IPC_STAT");
		shmctl(shmid, IPC_RMID, NULL);
		return 1;
	}

	printf("New mode: %o\n", ds.shm_perm.mode);

	shmctl(shmid, IPC_RMID, NULL);

	return 0;
}
