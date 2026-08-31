# Shared Memory Experiment — Short Summary

* **Program 1 (Writer)**

  * `shmget(1234, 4096, ...)` → created/found shared memory.
  * Got `shmid = 5`.
  * `shmat()` returned a **virtual address** like `0x76f9...`.
  * Wrote `"Hello World..."` into shared memory.
  * Exited without `IPC_RMID`, so the segment remained.

* **Program 2 (Reader)**

  * `shmget(1234, ...)` → got the **same `shmid = 5`** because key `1234` already existed.
  * `shmat()` returned a **different virtual address** like `0x71a4...`.
  * Still read the same `"Hello World..."` data.

## Key Point

```text
Process 1                    Process 2
0x76f9...                    0x71a4...
    │                            │
    ▼                            ▼
 Page Table                   Page Table
    │                            │
    └──────────┬─────────────────┘
               ▼
        Same Physical Memory
```

* `shmat()` → returns a **virtual address**, not physical address.
* Different processes can have different virtual addresses for the **same shared memory**.
* `shmid` remains the same.
* Shared memory survives the creator process exiting until it is removed.
* `shmctl(shmid, IPC_RMID, NULL)` → removes the shared-memory segment.

## Main lessons
- key identifies the shared-memory segment.
- shmid is the ID returned by shmget().
- shmat() returns a virtual address.
- Different processes can get different virtual addresses for the same segment.
- Those virtual addresses can map to the same physical memory.
- Shared-memory data survives the creator process's exit if the segment hasn't been removed.
- nattch tells you how many processes are currently attached.
- IPC_RMID is used to remove the shared-memory segment.
- Shared memory provides shared data, but it does not automatically provide synchronization

#### You can actually see the shared memory in linux using commands.

```sh
ipcs 
```

although it will show the message queues and semaphores.