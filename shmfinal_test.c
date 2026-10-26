#include "types.h"
#include "user.h"
#include "shm.h"

#define PG 4096

int base;
int id;
int passed;
int failed;
char *a; //attach mem 1st
char *b; //use to attach same mem 2nd time

void verify(char *name, int isok) {
	if (isok) {
		passed++;
		printf(1, "%s pass\n", name);
	} else {
		failed++;
		printf(1, "%s fail\n", name);
	}
}

int badaddr(char *p) {
	return p == (char *)-1 || p == 0;
}

void itos(int n, char *s) {
	int i = 0, j;
	char t;

	while (n > 0) {
		s[i++] = '0' + n % 10;
		n /= 10;
	}
	s[i] = 0;

	for (j = 0; j < i / 2; j++) {
		t = s[j];
		s[j] = s[i - j - 1];
		s[i - j - 1] = t;
	}
}

void test_get() {
	int x;

	x = shmget(base + 1, PG, IPC_CREAT | 0600);
	int created = x >= 0;
	verify("shmget.create.newkey", created);
	if (x >= 0) {
		shmctl(x, IPC_RMID, 0);
	}

	x = shmget(base, PG, 0);
	int reopened = x == id;
	verify("shmget.open.existing.key", reopened);

	x = shmget(base, PG, IPC_CREAT | IPC_EXCL | 0600);
	int exclRejected = x < 0;
	verify("shmget.excl.existing.key", exclRejected);

	x = shmget(base + 2, PG, 0);
	int missingRejected = x < 0;
	verify("shmget.open.missing.key", missingRejected);

	x = shmget(base + 3, 0, IPC_CREAT | 0600);
	int zeroRejected = x < 0;
	verify("shmget.create.zero.size", zeroRejected);

	x = shmget(base + 4, -1, IPC_CREAT | 0600);
	int negativeRejected = x < 0;
	verify("shmget.create.negative.size", negativeRejected);

	x = shmget(base + 5, (SHM_MAX_PAGES + 1) * PG, IPC_CREAT | 0600);
	int oversizeRejected = x < 0;
	verify("shmget.create.oversize", oversizeRejected);
}

void test_at() {
	char *p;

	a = (char *)shmat(id, 0, 0);
	int firstAttached = !badaddr(a);
	verify("shmat.attach.valid", firstAttached);

	b = (char *)shmat(id, 0, 0);
	int secondAttached = !badaddr(b);
	verify("shmat.attach.same.segment.twice", secondAttached);

	if (firstAttached && secondAttached) {
		a[0] = 'A';
		int firstPageShared = b[0] == 'A';
		verify("shmat.write.visible.across.mappings", firstPageShared);
		a[PG] = 'B';
		int secondPageShared = b[PG] == 'B';
		verify("shmat.write.second.page.shared", secondPageShared);
	} else {
		verify("shmat.write.visible.across.mappings", 0);
		verify("shmat.write.second.page.shared", 0);
	}

	p = (char *)shmat(-1, 0, 0);
	int invalidIdRejected = badaddr(p);
	verify("shmat.attach.invalid.id", invalidIdRejected);
	if (!badaddr(p)) {
		shmdt(p);
	}

	p = (char *)shmat(id, 0, 0x40000000);
	int invalidFlagsRejected = badaddr(p);
	verify("shmat.attach.invalid.flags", invalidFlagsRejected);
	if (!badaddr(p)) {
		shmdt(p);
	}

	int secondDetached = !badaddr(b) && shmdt(b) == 0;
	verify("shmdt.second.mapping.detach", secondDetached);
	b = 0;

	int wrongAddress = !badaddr(a) && shmdt(a + 1) < 0;
	verify("shmdt.wrong.address.reject", wrongAddress);

	int firstDetached = !badaddr(a) && shmdt(a) == 0;
	verify("shmdt.first.mapping.detach", firstDetached);
	a = 0;

	int detachedAgainRejected = shmdt(0) != 0;
	verify("shmdt.already.detached.reject", detachedAgainRejected);
}

void test_remap() {
	char *p;
	int id2;
	void *fixed = (void *)0x70020000;

	id2 = shmget(base + 6, PG, IPC_CREAT | 0600);
	int secondCreated = id2 >= 0;
	verify("shmat.remap.create.second.segment", secondCreated);
	if (!secondCreated) {
		return;
	}

	p = (char *)shmat(id, fixed, 0);
	int fixedAttached = !badaddr(p);
	verify("shmat.fixed.addr.attach", fixedAttached);
	if (!fixedAttached) {
		shmctl(id2, IPC_RMID, 0);
		return;
	}

	p[0] = 'M';
	p = (char *)shmat(id2, fixed, SHM_REMAP);
	int remapped = !badaddr(p);
	verify("shmat.remap.same.address", remapped);
	if (remapped) {
		int zeroed = p[0] == 0;
		verify("shmat.remap.zeroed", zeroed);
		int remapDetached = shmdt(p) == 0;
		verify("shmat.remap.detach", remapDetached);
	}

	shmctl(id2, IPC_RMID, 0);
}

void test_dt() {
	char *p;

	p = (char *)shmat(id, 0, 0);
	int attached = !badaddr(p);
	verify("shmdt.attach.valid", attached);
	if (!attached) {
		return;
	}

	int wrongBaseRejected = shmdt(p + 1) < 0;
	verify("shmdt.wrongbase.reject", wrongBaseRejected);

	int detached = shmdt(p) == 0;
	verify("shmdt.valid.detach", detached);

	int twiceRejected = shmdt(p) < 0;
	verify("shmdt.twice.reject", twiceRejected);
}

void test_ctl() {
	union {
		struct shmctl_ds ds;
		struct shmctl_info info;
	} buf;
	int r;

	r = shmctl(id, IPC_STAT, &buf);
	int statOk = r == 0;
	verify("shmctl.stat.valid", statOk);

	int sizeOk = r == 0 && buf.ds.size == 2 * PG;
	verify("shmctl.stat.size", sizeOk);

	int metadataOk = r == 0 && buf.ds.shmid == id && buf.ds.npages == 2 &&
		buf.ds.creator_pid == getpid() && buf.ds.marked_for_removal == 0;
	verify("shmctl.stat.metadata", metadataOk);

	int nullRejected = shmctl(id, IPC_STAT, 0) < 0;
	verify("shmctl.stat.nullbuffer", nullRejected);

	int badIdRejected = shmctl(-1, IPC_STAT, &buf) < 0;
	verify("shmctl.stat.invalidid", badIdRejected);

	int badCommandRejected = shmctl(id, 999, &buf) < 0;
	verify("shmctl.invalid.command.reject", badCommandRejected);

	buf.ds.mode = 0640;
	r = shmctl(id, IPC_SET, &buf);
	int setOk = r == 0;
	verify("shmctl.set.mode", setOk);

	r = shmctl(id, IPC_STAT, &buf);
	int modeUpdated = r == 0 && buf.ds.mode == 0640;
	verify("shmctl.stat.mode.updated", modeUpdated);

	r = shmctl(id, IPC_INFO, &buf);
	int ipcInfoRead = r >= 0;
	verify("shmctl.ipcinfo.read", ipcInfoRead);

	int limitsOk = r >= 0 && (buf.info.max_segments > 0) &&
		(buf.info.max_pages_per_segment > 0) &&
		(buf.info.max_total_pages ==
		(buf.info.max_segments * buf.info.max_pages_per_segment));
	verify("shmctl.ipcinfo.limits", limitsOk);

	r = shmctl(id, SHM_INFO, &buf);
	int shmInfoRead = r >= 0;
	verify("shmctl.shminfo.read", shmInfoRead);
	int activeOk = r >= 0 && buf.info.used_segments > 0 &&
		buf.info.total_pages >= 2;
	verify("shmctl.shminfo.active.segments", activeOk);

	int attachmentsOk = r >= 0 && buf.info.total_attachments >= 0 &&
		buf.info.highest_index >= 0;
	verify("shmctl.shminfo.attachments", attachmentsOk);

	r = shmctl(id - 1, SHM_STAT, &buf);
	int statIndexOk = r == id;
	verify("shmctl.shmstat.index", statIndexOk);
	r = shmctl(id - 1, SHM_STAT_ANY, &buf);

	int statAnyIndexOk = r == id;

	verify("shmctl.shmstat.any.index", statAnyIndexOk);

	int badIndexRejected = shmctl(-1, SHM_STAT, &buf) < 0;
	verify("shmctl.shmstat.invalid.index", badIndexRejected);
}

void test_fork() {
	int pid;

	a = (char *)shmat(id, 0, 0);
	int attached = !badaddr(a);
	verify("shmfork.attach.before.fork", attached);
	if (!attached) {
		return;
	}

	a[16] = 0;
	pid = fork();
	if (pid == 0) {
		a[16] = 'C';
		exit();
	}
	int childCreated = pid >= 0;
	verify("shmfork.child.create", childCreated);

	int childWaited = pid >= 0 && wait() == pid;
	verify("shmfork.child.wait", childWaited);

	int writeVisible = a[16] == 'C';
	verify("shmfork.child.write.visible", writeVisible);

	int detached = shmdt(a) == 0;
	verify("shmfork.parent.detach", detached);
	a = 0;
}

void test_exec() {
	int pid;
	char *args[4];
	char key[12];

	a = (char *)shmat(id, 0, 0);
	int attached = !badaddr(a);
	verify("shmexec.attach.before.exec", attached);
	if (!attached) {
		return;
	}

	a[32] = 'E';
	itos(base, key);
	pid = fork();
	if (pid == 0) {
		args[0] = "shmfinal_test";
		args[1] = "child";
		args[2] = key;
		args[3] = 0;
		exec("shmfinal_test", args); //because of this. other file starting the test file again run.
		exit();
	}

	int childCreated = pid >= 0;
	verify("shmexec.child.create", childCreated);
	int childWaited = pid >= 0 && wait() == pid;
	verify("shmexec.child.wait", childWaited);

	int mappingRemains = a[32] == 'E';
	verify("shmexec.parent.mapping.remains", mappingRemains);

	int detached = shmdt(a) == 0;
	verify("shmexec.parent.detach", detached);
	a = 0;
}

void test_rm() {
	struct shmctl_ds ds;

	int removed = shmctl(id, IPC_RMID, 0) == 0;
	verify("shmctl.rmid.remove", removed);

	int statRejected = shmctl(id, IPC_STAT, &ds) < 0;
	verify("shmctl.stat.removed", statRejected);
}

int main(int argc, char *argv[]) {

	if (argc > 1 && strcmp(argv[1], "child") == 0) {
		int cid;
		char *m;

		cid = argc > 2 ? shmget(atoi(argv[2]), PG, 0) : -1;
		m = cid < 0 ? (char *)-1 : (char *)shmat(cid, 0, 0);
		int seesData = !badaddr(m) && m[32] == 'E';
		verify("shmexec.child.attach.after.exec", seesData);
		if (!badaddr(m)) {
			shmdt(m);
		}
		exit();
	}

	base = 0x5a0000 + getpid(); // this is used to get unique key at every run.
	id = shmget(base, 2 * PG, IPC_CREAT | 0600);
	int setupOk = id >= 0;
	verify("shm.setup.create.segment", setupOk); //setupOk for parent run only.
	if (setupOk) {
		test_get();
		test_at();
		test_remap();
		test_dt();
		test_ctl();
		test_fork();
		test_exec();
		test_rm();
	}

	printf(1, "\n\npassed: %d\nfailed: %d\n", passed, failed);
	exit();
}
