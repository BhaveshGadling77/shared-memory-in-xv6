#include<stdio.h>
#include<stdlib.h>
#include<sys/shm.h>
#include<sys/wait.h>
#include<unistd.h>
#include<fcntl.h>
#include <string.h>
typedef struct student {
	int roll;
	char name[32];
	float marks;
} student;

int main() {
		
	int shmid = shmget(1234, 4096, IPC_CREAT | 0666);
	student s = {
    	.roll = 77,
    	.name = "Bhavesh",
    	.marks = 99.9
	};

	if (shmid == -1) {
		perror("shmget");
		exit(1);
	}

	printf("shared memory ID: %d\n", shmid);

	char *ptr = (char *)shmat(shmid, NULL, 0);

	memcpy(ptr, &s, sizeof(student));

	pid_t pid = fork();
	
	if (pid == -1) {
		printf("Unable to allocate process.\n");
		exit(1);
	}
	if (pid == 0) {
		student s1;
		memcpy(&s1, ptr, sizeof(student));

		printf("The student is having rollno: %d\nname: %s\nmarks: %f\nshmloc: %p\n",
		 				s.roll, s.name, s.marks, ptr);
		
		//curiosity
		s1.marks = 90.9;
		memcpy(ptr, &s1, sizeof(student));
	} else {
		wait(NULL);

		student s2;
    	memcpy(&s2, ptr, sizeof(student));

    	printf("Parent: marks = %.2f\nshmloc: %p\n", s2.marks, ptr);
		shmdt(ptr);
		shmctl(shmid, IPC_RMID, NULL);
	}
	return 0;

}
