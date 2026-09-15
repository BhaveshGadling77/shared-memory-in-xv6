#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>

int main()
{
    key_t key1, key2, key3;

    key1 = ftok(".", 'A');
    key2 = ftok(".", 'B');
    key3 = ftok("/tmp", 'A');

    if (key1 == -1 || key2 == -1 || key3 == -1) {
        perror("ftok");
        exit(EXIT_FAILURE);
    }

    printf("Same path, Project ID A: %d\n", key1);
    printf("Same path, Project ID B: %d\n", key2);
    printf("Different path, Project ID A: %d\n", key3);

    return 0;
}