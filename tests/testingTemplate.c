#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
/* checking if open() syscall works 
* open takes three arguments: pathname, flags, mode
* vary pathname: correct path, non-existent path
        pathname of a file with r permission, without r permissions
        pathname of a file with w permission, without w permissions
*/
struct testcase {
    char testname[16];
    char filename[16];
    int flags;
    int expected_result;
} testcases[] =  {
    { "open.1",
      "/etc/passwd",
      O_RDONLY,
      3
    },
    { "open.2",
      "/etc/passwd",
      O_WRONLY,
     -1
    },
    { "open.3",
      "/etc/dfdf",
      O_WRONLY,
     -1
    }
};
char testname[16];
int pass() {
    printf("%s pass\n", testname);
}
int fail() {
    printf("%s fail\n", testname);
}
int warn() {
    printf("%s warn\n", testname);
}
void test(struct testcase t) {
    int fd;
    fd = open(t.filename, t.flags);
    strcpy(testname, t.testname);
    if(fd == t.expected_result) {
        pass();
    }
    else  {
        fail();
    }
}
int main(int argc, char *argv[]) {
 
    int i;
    for(i = 0; i < sizeof(testcases)/sizeof(struct testcase) ; i++)
        test(testcases[i]);
 
}