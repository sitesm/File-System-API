# Assignment #4 – The “File System 311” - FS3 Filesystem (Version 3.0) - CMPSC311 - Introduction to Systems Programming - Fall 2021 - Prof. McDaniel

## **Due date: Friday, December 10 (11:59pm EDT)**

**Note: THERE WILL BE NO EXTENSION OR LATE PENALTIES SUBMISSIONS ACCEPTED FOR ASSIGNMENT #4**

**We have to get everything graded by the end of finals week.**

---
## Prerequisite

- Refer to assignment #2.
- Download the three workload files from canvas:
  - assign4-jumbo-workload.txt
  - assign4-medium-workload.txt
  - assign4-small-workload.txt

---
## Description

Assignment #4 will continue our implementation of the device driver for the FS3 filesystem. Note that you will be required to use your code from the previous assignment (#3) as the starting place for the next assignment. 

Unless stated otherwise, all the operations and specifics from assignment #3 apply to assignment #4. None of the APIs you used in the previous assignment have changed.

---
## System and Project Overview

In this assignment you are to modify your driver code to connect over the network to another machine on which the disk controller runs. You are to write the corresponding network code in the file `fs3_network.c` whose declarations are made in `fs3_network.h`. The function to write is:

`int network_fs3_syscall(FS3CmdBlk cmd, FS3CmdBlk *ret, void *buf);`
```
Params:
  cmd - as previous assignments
  ret - pointer to place where to put the returned command block (with ret bit)
  buf - as previous assignments
Returns:
  0 if successful, -1 if _network_ failure
  ```
  
This function is the client/network system call, you should connect over the network to communicate with the disk controller. Connect to the disk controller when a `mount` operation needs to be performed and disconnect when performing an `unmount` operation.

In the rest of your code, you will need to replace all your calls to `fs3_syscall` by a call to `network_fs3_syscall`. 

You should use the IP address and port defined in `fs3_network.h` to setup your networking socket: 
 - IP address: `extern unsigned char *fs3_network_address;` if this is `NULL` use `FS3_DEFAULT_IP`.
 - Port: `extern unsigned short fs3_network_port;` if this is `0`, use `FS3_DEFAULT_PORT`.
  
**Note:** the server network code for the disk controller is already implemented, you only need to implement the client network code for your driver to connect to the disk controller. Your `network_fs3_syscall` function should connect to the server when necessary, send the corresponding command block and buffer (if non `NULL`) back to back at once, and receive the command block and buffer (depending on the operation being performed) returned by the disk controller through the socket.

---
## Workloads

There are 3 different workloads for this assignment:

- `assign4-small-workload.txt` - 15 files (up 350k bytes), 400k+ operations
  - This took about 46 seconds on high-end server.

- `assign4-medium-workload.txt` - 159 files (up to 400k bytes), 4M operations
  - This took about 11 min, 38 seconds on high end server

- `assign4-jumbo-workload.txt` - ?? files (up to ???k bytes), 6M+ operations
  - This took about 18 minutes, 53 seconds on high end server

**Note:** logs may get very large, you may want to either disable them, delete them between runs, or increase disk space of your VM. Similarly, you may want to increase the resources allocated to your VM (more CPU cores, RAM, and disk) to speed up the simulation run if things are too slow. 

---
## Schreyer Honors College Option 

Modify your code to hash the data you have on each sector (using something like the `gcrypt MD5` function, see `cmpsc311_util.h`). This is used in real applications for integrity purpose to check that the data stored is not corrupted. Adapt your writes and reads accordingly to create/update this hash or verify that the data returned by the disk controller is not corrupted (if that is the case you should return an error in read/write).

Hint: you will need to reserve some space on each sector to store the hash.


---
## How to compile and test

- To cleanup your compiled objects, run:
  ```
  make clean
  ```

- To compile your code, you have to use:
  ```
  make
  ```

- To run the server:
  ```
  ./fs3_server -v -l fs3_server_log.txt 
  OR
  ./fs3_server
  ```

**Note:** you need to restart the server each time you run the client.
**Note:** when you use the `-l` argument, you will see `*` appear every so often. Each dot represents 100k workload operations. This allows you to see how things are moving along.

- To run the client:
  ```
  ./fs3_client -v -l fs3_client_log_small.txt assign4-small-workload.txt
  ./fs3_client -v -l fs3_client_log_medium.txt assign4-medium-workload.txt
  ./fs3_client -v -l fs3_client_log_jumbo.txt assign4-jumbo-workload.txt
  ```

- If the program completes successfully, the following should be displayed as the last log entry:
  ```
  FS3 simulation: all tests successful!!!
  ```

---
## How to turn in

1. Submit on Canvas both the commit ID that should be graded, as well as your GitHub username. Otherwise you will receive a 0 for the assignment.
2. Before submitting the commit ID, you can test that the TAs will get the correct version of your code by cloning again your GitHub repository in ANOTHER directory on your VM and check that everything compiles as expected.

---
**Note:** *Like all assignments in this class you are prohibited from copying any content from the Internet or discussing, sharing ideas, code, configuration, text or anything else or getting help from anyone in or outside of the class. Consulting online sources is acceptable, but under no circumstances should anything be copied. Failure to abide by this requirement will result dismissal from the class as described in our [course syllabus](https://psu.instructure.com/courses/2136848/assignments/syllabus).*

