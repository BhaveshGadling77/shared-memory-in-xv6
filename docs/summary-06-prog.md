# Summary

- if we use the SHM_RDONLY then it makes the shared memory attachment readonly.
- The process can read the memory but cannot write to it.
- because, the OS enforces the memory protection, not the C pointer type.

