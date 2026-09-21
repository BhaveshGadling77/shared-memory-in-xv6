
## shmget 

- case 1:
You can't allocate the shared memory of **Size = 0**. 
**It should fail in implementation**.

- case 2:
if the key dosen't exist in host and you used the key that is not pointing to memory then it should fail.

- case 3: 
**IPC_EXCL** will create through an error if the segment exists. 
however, I still didn't get why it thrown error of file exists(that is also an mystery)

- case 4:
IPC\_STAT is there get the perms of the current segment if you use it in shmctl
IPC\_SET is used to set the permission again to the segment.

we need to check the permissions on segment to implement the exact same behaviour.

- case 5: 
we have to implement the what will happen at exhaustion of memory.


## shmctl

- case 1:
the shmctl should check the whether the segment already exist or not.

- case 2:
invalid command should get the check and return -1;

- case 3: idon't know if we need to implement the ipc-stat structure.

- case 4: what permission can be modified.

