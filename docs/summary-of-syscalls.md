# System V Shared Memory — Simple Summary

## 1. What is Shared Memory?

Normally, every process has its **own address space**.

```text
Process A                    Process B

   Memory                       Memory
┌─────────────┐             ┌─────────────┐
│ code        │             │ code        │
│ data        │             │ data        │
│ heap        │             │ heap        │
│ stack       │             │ stack       │
└─────────────┘             └─────────────┘
```

So if Process A changes a variable, Process B normally cannot see it.

Shared memory allows multiple processes to access the **same physical memory**.

```text
Process A                  Process B
    │                          │
    │                          │
    └──────────┐    ┌──────────┘
               ▼    ▼
          ┌──────────────┐
          │ Shared       │
          │ Memory       │
          └──────────────┘
```

Both processes can read and write the same data.

---

# 2. Shared Memory Does NOT Automatically Provide Synchronization

This is very important.

Shared memory only solves:

> "How can two processes access the same memory?"

It does **not** solve:

> "How can they access it safely at the same time?"

For example:

```c
counter++;
```

is not necessarily one machine operation.

It is approximately:

```text
READ counter
ADD 1
WRITE counter
```

If two processes do this simultaneously:

```text
Initial counter = 0

Process A              Process B

READ 0
                       READ 0
ADD 1
                       ADD 1
WRITE 1
                       WRITE 1
```

Final result:

```text
counter = 1
```

instead of:

```text
counter = 2
```

This is called a **race condition**.

---

# 3. How to Prevent Data Corruption

Use a synchronization mechanism such as:

* Mutex
* Semaphore
* Atomic operations
* Other synchronization primitives

Conceptually:

```c
lock();

counter++;

unlock();
```

Then:

```text
Process A                  Process B

lock()
  │
  ▼
counter++
  │
  ▼
unlock()
                            │
                            ▼
                          lock()
                            │
                            ▼
                         counter++
                            │
                            ▼
                          unlock()
```

Only one process modifies the data at a time.

---

# 4. Two Different Types of Synchronization

There are two separate things to protect.

## 4.1 Kernel's Shared-Memory Metadata

The kernel maintains information about shared-memory segments, such as:

```text
key
shmid
size
attachments
status
memory information
```

Multiple processes may call `shmget()`, `shmat()`, etc. at the same time.

Therefore, the **kernel's shared-memory data structures need synchronization**.

For example, the kernel may use a lock.

---

## 4.2 Actual Data Inside Shared Memory

Suppose the shared memory contains:

```text
counter = 100
```

A kernel lock around `shmget()` does **not** protect this counter.

Why?

Because after `shmat()`:

```c
ptr->counter++;
```

happens directly in the process's address space.

Therefore, the application itself needs a synchronization mechanism if multiple processes access the same data concurrently.

### Important rule

> **Kernel locks protect the shared-memory subsystem. User-level synchronization protects the application's shared data.**

---

# 5. The Four Important System Calls

System V shared memory mainly uses:

```text
shmget()
shmat()
shmdt()
shmctl()
```

Their basic meanings are:

| System call | Simple meaning                         |
| ----------- | -------------------------------------- |
| `shmget()`  | Find or create a shared-memory segment |
| `shmat()`   | Attach the segment to a process        |
| `shmdt()`   | Detach the segment from a process      |
| `shmctl()`  | Control/manage/remove the segment      |

---

# 6. `shmget()`

Basic form:

```c
int shmget(key_t key, size_t size, int shmflg);
```

Example:

```c
int shmid = shmget(1234, 4096, IPC_CREAT | 0666);
```

There are three important arguments.

## 6.1 `key`

Example:

```c
shmget(1234, 4096, IPC_CREAT);
       ^^^^
       key
```

The `key` is basically a **name/identifier that processes can use to refer to the same shared-memory segment**.

For example:

```text
SHM Table

KEY     SHMID     SIZE
-----------------------
100       0       4096
200       1       8192
300       2       4096
```

If a process calls:

```c
shmget(200, ...);
```

the kernel can find the segment associated with key `200`.

---

# 7. Key vs SHMID

These are different things.

```text
KEY                         SHMID
 │                            │
 │ used by shmget()           │ returned by shmget()
 │                            │
1234                          5
```

For example:

```c
int shmid = shmget(1234, 4096, IPC_CREAT);
```

could produce:

```text
key   = 1234
shmid = 5
```

Then:

```c
shmat(5, NULL, 0);
```

uses the **shmid**, not the key.

### Remember:

> **Key → used to find/create the segment**

> **SHMID → ID returned by `shmget()` and used by other operations**

---

# 8. `shmget()` — `size`

Example:

```c
shmget(1234, 4096, IPC_CREAT);
             ^^^^
             size
```

This specifies the size of the shared-memory segment.

For example:

```text
4096 bytes  = 4 KB
8192 bytes  = 8 KB
```

The actual allocation is handled by the operating system.

---

# 9. `shmget()` — Flags

Example:

```c
shmget(1234, 4096, IPC_CREAT | 0666);
                      ^^^^^^^^^^^^^^^
                         flags
```

Flags tell the kernel how the operation should behave.

The important one is:

```c
IPC_CREAT
```

It means approximately:

> If the segment does not exist, create it.

So:

```text
Does key 1234 exist?
        │
   ┌────┴────┐
  YES        NO
   │          │
   ▼          ▼
return      create
existing    segment
segment
```

`0666` represents permissions similar to Unix file permissions.

---

# 10. `shmat()`

Basic form:

```c
void *shmat(int shmid, const void *shmaddr, int shmflg);
```

Example:

```c
char *ptr = shmat(shmid, NULL, 0);
```

Its purpose is:

> Attach a shared-memory segment to the process's address space.

---

## 10.1 `shmid`

This is the ID returned by `shmget()`.

```c
int shmid = shmget(...);

char *ptr = shmat(shmid, NULL, 0);
```

---

## 10.2 `shmaddr`

This specifies the virtual address where the shared memory should be attached.

Usually:

```c
NULL
```

means:

> Let the kernel choose a suitable address.

For example:

```text
Process A                    Process B

Virtual address              Virtual address
0x5000                       0x8000
   │                             │
   └──────────┐     ┌────────────┘
              ▼     ▼
            Same physical memory
```

The virtual addresses can be different while pointing to the same physical pages.

---

## 10.3 `shmflg`

Controls attachment behavior.

For a simple usage:

```c
shmat(shmid, NULL, 0);
```

uses the default behavior.

---

# 11. `shmdt()`

Basic form:

```c
int shmdt(const void *shmaddr);
```

Example:

```c
char *ptr = shmat(shmid, NULL, 0);

...

shmdt(ptr);
```

The argument is the **address returned by `shmat()`**.

It means:

> Remove this process's attachment to the shared-memory segment.

Important:

> `shmdt()` does NOT necessarily destroy the shared-memory segment.

It only removes that process's mapping/attachment.

---

# 12. `shmctl()`

Basic form:

```c
int shmctl(int shmid, int cmd, struct shmid_ds *buf);
```

Think of it as:

```text
shmctl(ID, COMMAND, INFORMATION)
```

## 12.1 `shmid`

The shared-memory ID returned by `shmget()`.

---

## 12.2 `cmd`

Specifies what operation should be performed.

One important command is:

```c
IPC_RMID
```

which means:

> Mark the shared-memory segment for removal.

Example:

```c
shmctl(shmid, IPC_RMID, NULL);
```

---

## 12.3 `buf`

Used by some `shmctl()` commands to pass information between the user process and kernel.

For removal:

```c
shmctl(shmid, IPC_RMID, NULL);
```

`NULL` is sufficient because no information structure is needed for that operation.

---

# 13. Complete Shared-Memory Flow

A typical lifecycle looks like:

```c
// 1. Find/create
int shmid = shmget(1234, 4096, IPC_CREAT | 0666);

// 2. Attach
char *ptr = shmat(shmid, NULL, 0);

// 3. Use shared memory
ptr[0] = 'A';

// 4. Detach
shmdt(ptr);

// 5. Remove/control
shmctl(shmid, IPC_RMID, NULL);
```

The information flow is:

```text
KEY
 │
 │ shmget()
 ▼
SHMID
 │
 │ shmat()
 ▼
ADDRESS
 │
 │ use shared memory
 ▼
shmdt()
 │
 │
 ▼
shmctl()
```

### Most important chain

> **KEY → `shmget()` → SHMID → `shmat()` → ADDRESS → `shmdt()`**

---

# 14. Do Processes Need to Be Related?

**No.**

Processes do NOT need to have a parent-child relationship.

They can be completely unrelated.

For example:

```text
Terminal 1                    Terminal 2

Process A                     Process B
    │                             │
    │ shmget(1234, ...)           │
    │                             │
    └──────────────┐   ┌──────────┘
                   ▼   ▼
                Same SHM
```

Both can use the same key:

```c
shmget(1234, ...);
```

and obtain the same shared-memory segment.

---

# 15. What About `fork()`?

`fork()` is simply one way to create related processes.

For example:

```text
Parent
  │
  │ fork()
  ├─────────┐
  ▼         ▼
Parent     Child
```

They can then use shared memory.

But:

> **`fork()` is NOT required for System V shared memory.**

The shared-memory relationship is:

```text
Process A ──────┐
Process B ──────┼──> SHM Segment
Process C ──────┘
```

A, B and C can be completely unrelated.

---

# 16. How Long Does Shared Memory Exist?

This is one of the most important concepts.

The shared-memory segment has a lifetime **separate from the process that created it**.

Therefore:

```text
Process A creates SHM
        │
        ▼
Process A exits
        │
        ▼
SHM can still exist
```

The creator's exit does NOT automatically destroy the shared-memory segment.

---

# 17. `shmdt()` Does Not Mean "Destroy"

Suppose:

```text
Process A ──────┐
                ▼
             SHM
                ▲
Process B ──────┘
```

A calls:

```c
shmdt(address);
```

Now:

```text
Process A

(no attachment)

Process B ───────> SHM
```

The shared memory is still needed by B.

Therefore:

> **`shmdt()` only removes one process's attachment.**

It does not necessarily free the physical memory.

---

# 18. What Happens When All Processes Detach?

Suppose:

```text
Process A ───────> SHM
Process B ───────> SHM
```

A detaches:

```text
Process B ───────> SHM
```

B then detaches:

```text
No processes attached
       │
       ▼
      SHM
```

Does the kernel automatically destroy it?

**Not necessarily.**

The segment can still exist even with zero attachments if it has not been removed.

---

# 19. `IPC_RMID`

To remove the shared-memory segment:

```c
shmctl(shmid, IPC_RMID, NULL);
```

This tells the kernel:

> This shared-memory segment should be removed.

If processes are still attached, the segment is not simply made unusable immediately.

Conceptually:

```text
SHM segment
     │
     ├── marked for removal
     │
     └── processes may still be attached
```

Once the remaining attachments are gone, the segment can be destroyed and its resources reclaimed.

---

# 20. When Is Physical Memory Reclaimed?

For the simple mental model:

> **The physical resources are reclaimed when the shared-memory segment is actually destroyed.**

Typically:

```text
IPC_RMID
   +
no remaining attachments
   ↓
segment destroyed
   ↓
physical resources reclaimed
```

Therefore:

### Case 1: `shmdt()`

```text
Process detaches
```

→ ❌ Physical memory is not necessarily freed.

---

### Case 2: Creator exits

```text
Creator process exits
```

→ ❌ Shared memory is not automatically destroyed.

---

### Case 3: `IPC_RMID` while processes are attached

```text
IPC_RMID
+
attachments > 0
```

→ ⏳ Segment remains until the relevant attachments are gone.

---

### Case 4: `IPC_RMID` + zero attachments

```text
IPC_RMID
+
attachments = 0
```

→ ✅ Segment can be destroyed and resources reclaimed.

---

### Case 5: Nobody calls `IPC_RMID`

```text
attachments = 0
+
not marked for removal
```

→ ❌ Segment can remain in the system.

It may continue consuming system resources until explicitly removed or otherwise cleaned up.

---

# 21. Simple Lifecycle Diagram

```text
              shmget()
                  │
                  ▼
          ┌───────────────┐
          │ SHM Segment   │
          └───────┬───────┘
                  │
                shmat()
                  │
                  ▼
             Process uses
             shared memory
                  │
             ┌────┴────┐
             │         │
          shmdt()    Process exits
             │         │
             └────┬────┘
                  │
                  ▼
          Segment can still
               exist
                  │
                  ▼
          shmctl(IPC_RMID)
                  │
                  ▼
         Marked for removal
                  │
                  ▼
        No remaining users?
             │          │
            NO         YES
             │          │
             ▼          ▼
           Wait      Destroy
                        │
                        ▼
              Resources reclaimed
```

---

# 22. System Calls vs Blocking

Another important point:

> **A system call is not automatically a blocking system call.**

`shmget()`, `shmat()`, `shmdt()`, and `shmctl()` are **not normally considered blocking system calls**.

For example:

```text
shmget()
   │
   ├── validate arguments
   ├── find/create segment
   └── return
```

It doesn't normally wait for another process to produce some data.

Compare this with:

```text
read()
wait()
sleep()
```

which can cause a process to wait.

---

# 23. What About Locks Inside These System Calls?

A shared-memory system call may internally use a lock to protect kernel data structures.

For example:

```text
Process A              Process B

shmget()               shmget()
   │                      │
   ▼                      ▼
acquire lock          waits/spins
   │
   ▼
modify SHM table
   │
   ▼
release lock
                          │
                          ▼
                       acquire lock
```

This does NOT mean:

> "`shmget()` is inherently a blocking system call."

The waiting is due to synchronization around the kernel's internal data structures.

---

# 24. Final Mental Model

Keep these three layers separate.

```text
┌─────────────────────────────────────┐
│ 1. SHARED MEMORY                    │
│                                     │
│ Allows processes to access the      │
│ same physical memory.               │
└─────────────────────────────────────┘
                  │
                  ▼
┌─────────────────────────────────────┐
│ 2. SHARED MEMORY SYSTEM CALLS       │
│                                     │
│ shmget() → find/create               │
│ shmat()  → attach                   │
│ shmdt()  → detach                   │
│ shmctl() → control/remove            │
└─────────────────────────────────────┘
                  │
                  ▼
┌─────────────────────────────────────┐
│ 3. SYNCHRONIZATION                  │
│                                     │
│ Prevents race conditions and        │
│ protects shared data.               │
│                                     │
│ mutex / semaphore / atomic ops      │
└─────────────────────────────────────┘
```

## The five things to remember

1. **Shared memory allows multiple processes to access the same physical memory.**

2. **Processes do not need to be related by `fork()`.**

3. **`shmget()` finds/creates the segment using a `key` and returns a `shmid`.**

4. **`shmat()` attaches the segment and gives the process an address; `shmdt()` removes that attachment.**

5. **`IPC_RMID` is what tells the kernel to remove the shared-memory segment; the resources are reclaimed when the segment is actually destroyed, after remaining attachments are gone.**

### The most important chain

```text
key
 ↓
shmget()
 ↓
shmid
 ↓
shmat()
 ↓
address
 ↓
use shared memory
 ↓
shmdt()
 ↓
shmctl(IPC_RMID)
 ↓
segment destroyed
 ↓
resources reclaimed
```
