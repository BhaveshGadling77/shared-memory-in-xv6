#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "shm.h"

#define PG 4096

int passed, failed;

void verify(char *name, int ok) {
    if(ok) {
      passed++;
      printf(1, "%s pass\n", name);
    } else {
      failed++;
      printf(1, "%s fail\n", name);
    }
}

int main(void) {
    char name[16];
    int n, i, fd, fd2, ro, truncfd, regularfd, pid;
    void *p, *q, *hintmap;

    name[0] = '/';
    n = getpid();
    i = 1;
    if (n == 0)
        name[i++] = '0';
    while (n > 0 && i < 14) {
        name[i++] = '0' + n % 10;
        n /= 10;
    }
    name[i++] = 's';
    name[i++] = 'm';
    name[i] = 0;

    verify("shm_open.invalid.name.no.leading.slash",
           shm_open("badname", O_CREAT|O_RDWR, 0600) < 0);
    verify("shm_open.invalid.name.nested.path",
           shm_open("/bad/name", O_CREAT|O_RDWR, 0600) < 0);
    verify("shm_open.missing.without.create",
           shm_open("/notthere", O_RDWR, 0) < 0);
    verify("mmap.invalid.fd",
           mmap(0, PG, PROT_READ, MAP_SHARED, -1, 0) == (void*)-1);

    fd = shm_open(name, O_CREAT|O_EXCL|O_RDWR, 0600);
    verify("shm_open.create", fd >= 0);

    if(fd < 0)
      exit();

    verify("shm_open.excl.existing",
           shm_open(name, O_CREAT|O_EXCL|O_RDWR, 0600) < 0);
    verify("shm_open.unsupported.flags",
           shm_open(name, O_RDWR|O_WRONLY, 0600) < 0);
    fd2 = shm_open(name, O_RDWR, 0);

    verify("shm_open.open.existing", fd2 >= 0);

    truncfd = shm_open(name, O_RDWR|O_TRUNC, 0);
    verify("shm_open.truncate.existing", truncfd >= 0);
    verify("shm_open.truncate.resets.size",
           truncfd < 0 || mmap(0, PG, PROT_READ, MAP_SHARED, truncfd, 0) == (void*)-1);
    if (truncfd >= 0)
        close(truncfd);

    regularfd = open("README", O_RDONLY);

    verify("mmap.rejects.regular.file",
           regularfd < 0 || mmap(0, PG, PROT_READ, MAP_SHARED, regularfd, 0) == (void*)-1);

    if (regularfd >= 0)
        close(regularfd);

    verify("ftruncate.zero.size", ftruncate(fd, 0) == 0);
	verify("mmap.zero.length",
       mmap(0, 0, PROT_READ, MAP_SHARED, fd, 0) == (void*)-1);
    verify("mmap.before.size",
           mmap(0, PG, PROT_READ, MAP_SHARED, fd, 0) == (void*)-1);
    verify("ftruncate.allocate", ftruncate(fd, 2*PG) == 0);

	ro = shm_open(name, 0, 0);
	verify("shm_open.readonly", ro >= 0);
	verify("ftruncate.readonly.reject", ro < 0 || ftruncate(ro, PG) < 0);
	verify("mmap.readonly.rejects.write",
			ro < 0 || mmap(0, PG, PROT_READ|PROT_WRITE, MAP_SHARED, ro, 0) == (void*)-1);
	verify("ftruncate.negative.reject", ftruncate(fd, -1) < 0);
	verify("ftruncate.oversize.reject",
			ftruncate(fd, (SHM_MAX_PAGES + 1)*PG) < 0);

	p = mmap(0, 2 * PG, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	q = fd2 < 0 ? (void*)-1 :
		mmap(0, 2 * PG, PROT_READ|PROT_WRITE, MAP_SHARED, fd2, 0);
	verify("mmap.shared.object", p != (void*)-1 && q != (void*)-1);

	if (p != (void*)-1 && q != (void*)-1) {
		((char*)p)[0] = 'P';
		((char*)q)[PG] = 'Q';
		verify("mmap.shared.pages.visible",
			((char*)q)[0] == 'P' && ((char*)p)[PG] == 'Q');

		pid = fork();

		if (pid == 0) {
			((char*)p)[1] = 'F';
			exit();
		}

		verify("mmap.fork.shared",
			pid > 0 && wait() == pid && ((char*)q)[1] == 'F');

		hintmap = mmap((void*)0x70010000, PG, PROT_READ, MAP_SHARED, fd, 0);
		verify("mmap.address.hint", hintmap != (void*)-1);
		if(hintmap != (void*)-1)
			shmdt(hintmap);
		verify("mmap.invalid.offset",
			mmap(0, PG, PROT_READ, MAP_SHARED, fd, 1) == (void*)-1);
		verify("mmap.invalid.flags",
			mmap(0, PG, PROT_READ, MAP_PRIVATE, fd, 0) == (void*)-1);
		verify("mmap.invalid.protection",
			mmap(0, PG, 8, MAP_SHARED, fd, 0) == (void*)-1);
		verify("mmap.fixed.unaligned.address",
			mmap((void*)0x70010001, PG, PROT_READ,
					MAP_SHARED|MAP_FIXED, fd, 0) == (void*)-1); //odd address
		verify("mmap.mapping.too.large",
			mmap(0, 3*PG, PROT_READ, MAP_SHARED, fd, 0) == (void*)-1);
		verify("ftruncate.shrink.rejects.old.range",
			ftruncate(fd, PG) == 0 &&
			mmap(0, 2*PG, PROT_READ, MAP_SHARED, fd, 0) == (void*)-1);
		verify("ftruncate.grow.zeroes.tail",
			ftruncate(fd, 2*PG) == 0 && ((char*)q)[PG] == 0);
	}

	verify("shm_unlink", unlink(name) == 0);
	verify("shm_open.after.unlink", shm_open(name, O_RDWR, 0) < 0);
	if (p != (void*)-1) {
		verify("mmap.live.after.unlink", ((char*)p)[0] == 'P');
		shmdt(p);
	}

	if (q != (void*)-1)
		shmdt(q);

	if (ro >= 0)
		close(ro);

	if (fd2 >= 0)
		close(fd2);

	close(fd);

	printf(1, "\nPOSIX shared memory: %d passed, %d failed\n", passed, failed);
	exit();
}
