### 1. A second `shmat(id, 0, 0)` always fails

`growproc()` and `exec()` keep `p->sz <= SHMBASE`, so `PGROUNDUP(p->sz) < SHMBASE` always holds. The auto-address therefore always becomes `0x70000000`. The pre-check then finds that PTE already present and returns `-1`.

c
```c
a = shmat(id1, 0, 0);   // 0x70000000, OK
b = shmat(id2, 0, 0);   // 0x70000000 again -> PTE_P set -> -1
```

**Your `shmattest` fails this way.** The child inherits the mapping at `0x70000000` through `shmfork`. Its own `shmat(shmid, 0, 0)` then returns `-1`:

c
```c
char *child = shmat(shmid, 0, 0);   // (char*)-1
*child = 100;                       // write to 0xFFFFFFFF -> page fault, child killed
```

The parent then prints `Parent: after child write = 0`, not 100.

---

### 2. Syscalls reject any pointer into shared memory

`argptr`, `argstr` and `fetchint` reject addresses `>= curproc->sz`. Shared memory starts at `0x70000000`, which is always above `sz`.

c
```c
char *buf = shmat(id, 0, 0);
write(1, buf, 5);          // returns -1
read(0, buf, 100);         // returns -1
printf(1, "%s\n", buf);    // printf goes through write() -> prints nothing
```

A chat program that reads input straight into shared memory, or prints a message from it, will fail.

---

### 3. `shmget()` on an existing key ignores the size

c
```c
a = shmget(1234, 4096,  IPC_CREAT);   // 1 page
b = shmget(1234, 32768, IPC_CREAT);   // same id, still 1 page
p = shmat(b, 0, 0);
p[5000] = 1;                          // page 2 is not mapped -> page fault, process killed
```

---

### 4. `IPC_EXCL` has no effect

c
```c
shmget(1234, 4096, IPC_CREAT);
id = shmget(1234, 4096, IPC_CREAT | IPC_EXCL);   // expected -1, returns the existing id
```

---

### 5. A huge `size` creates a segment with zero pages

`size + PGSIZE - 1` overflows `int` for `size` near `INT_MAX`.

c
```c
id = shmget(7, 0x7FFFFFFF, IPC_CREAT);   // succeeds, returns a valid id
```

`npages` wraps to about −524287, so the `npages > SHM_MAX_PAGES` check passes and the allocation loop never runs. You get a valid id for a segment with no pages, and `shmat` on it fails. It also uses one of the 16 slots.

---

### 6. `key == 0` (`IPC_PRIVATE`) is treated like any other key

c
```c
a = shmget(0, 4096, IPC_CREAT);
b = shmget(0, 4096, IPC_CREAT);   // same id as a; Linux returns two different segments
```

---

### 7. Segments are never freed, so `shmget` eventually fails permanently

The slots and their physical pages are never released, because there is no `shmdt` or `shmctl`, and `shmexit` only decrements `nattach`.
```
run shmtest 16 times (each uses a new key) -> NSHM slots full
17th run: shmget(...) returns -1 until reboot
```

Every run also permanently loses its pages from the free list.

---

### 8. `SHM_REMAP` always fails

c
```c
shmat(id, (void*)0x70000000, SHM_REMAP);   // always -1, even with a valid address
```

---

### 9. `SHM_EXEC` does nothing

Non-PAE x86 has no NX bit, so every `PTE_U` page is already executable.

c
```c
p = shmat(id, 0, SHM_EXEC);     // maps the same as flag 0
q = shmat(id, 0, 0);            // also executable
```

`shmexectest` can only show that the flag was accepted, not that it grants anything.

---

### 10. `exec` can overlap the stack with the shm region

The ELF check is `ph.vaddr + ph.memsz > SHMBASE`, so a binary ending exactly at `SHMBASE` passes. The two stack pages are then allocated above it, at `0x70000000–0x70002000`. A later `shmat` at or near `SHMBASE` collides with the stack: it fails when auto-addressed, and a fixed-address attach to that range also fails.

---

### 11. `shmget(key, 0, …)` on an existing key fails

`size <= 0` returns `-1` before the key lookup. Linux allows `size == 0` when you only want the id of an existing segment.