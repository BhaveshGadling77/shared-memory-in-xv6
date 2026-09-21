#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <string.h>

#define MAX_SEGMENTS 10000
#define SEGMENT_SIZE 4096

/* this is to test whether how does the shm syscalls work when memory exhaustion happened.
 */
int main(void) {
	int shmids[MAX_SEGMENTS];
	int count = 0;

	printf("started creating shared-memory segments...\n");

	while (count < MAX_SEGMENTS) {

		key_t key = 1 + count;

		int id = shmget(key, SEGMENT_SIZE, IPC_CREAT | IPC_EXCL | 0600);

		if (id == -1) {
			printf("\nCreation stopped.\n");
			printf("Segments created = %d\n", count);
			printf("errno: %d\n", errno);
			printf("error: %s\n", strerror(errno));
			break;
		}

		shmids[count] = id;
		count++;

		// it is just to print how many segments were created
		if (count % 100 == 0) 
			printf("created %d segments...\n", count);
	}

	for (int i = 0; i < count; i++) {
		if (shmctl(shmids[i], IPC_RMID, NULL) == -1) {
			perror("shmctl(IPC_RMID)");
		}
	}


	return 0;
}
