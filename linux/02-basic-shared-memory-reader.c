#include<stdio.h>
#include<stdlib.h>
#include<sys/ipc.h>
#include <sys/shm.h>

int main() {
	int shmid = shmget(1234, 4096, IPC_CREAT | 0666);

	if (shmid == -1) {
		perror("shmget");
		exit(1);
	}

	char *ptr = shmat(shmid, 0x50000000, 0); // i can use explicit typecasting of void*
	//instead of address u can use NULL to tell kernel 
	// to use whatever virtual address you want to attach 

	printf("at address %p written: %s", ptr, ptr);

	printf("Removing the shared memory!\n");
	shmctl(shmid, IPC_RMID, NULL); 
	//note i done that on purpose.
	
	
	return 0;
}
