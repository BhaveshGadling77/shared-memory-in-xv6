#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <sys/shm.h>
int main() {
    
	int shmid = shmget(1234, 4096, IPC_CREAT | 0666);

	if (shmid == -1) {
		perror("shmget");
		exit(1);
	}

	printf("shared memory ID: %d\n", shmid);

	char *ptr = (char *)shmat(shmid, (void *)0x50000000, 0);

	char *data = "Hello World written in shared memory segment!\n";
	strcpy(ptr, data);

	printf("at location %p the data written : %s\n", ptr, ptr);
	//note i hadn't use the shmctl that on purpose.
	return 0;
}
