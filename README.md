## Description
This is a file system I created during my junior year at Penn State University. This file system contains optimized algorithms, bit managment, caching functionality and network accessability. Over the course of 4 months, I wrote code, refactored said code, and continued to optimize it while adding new features.

In this project, I took the standard file operations (open, close, read, write, seek, etc...) and implimented them myself. The code is run by a simulator program that takes commands from the several workload files and test files. These files will place characters in specific locations to create a output file(.cmm), which, when finished, matches the file to test it against(.txt).

## System Calls
This project was meant to replicate a real program that would actualy interact with a medium of storage (mainly a FS3 Disk). This was accomplished by making systemcalls to the simulator, which in-turn then performed the desired task. These system calls send a command block, comprised of a operation code, track, and sector.

## Caching
Caching was a very important, and practical application of my programming skills. This cache can take any size, and has a Least-Recently-Used (LRU), Write-Through cache eviction policy. Before any read or write via systemcalls, the cache was checked for the desired data. If the data was found, the data was returned, if not, it was palced in the cache for later use.

## Network Accessability
This was the final feature that I implimented into this file system. Implimenting the network allowed for this program to be run through a server insetead of only on the local machine. This was very insigtful, because grasping the concept of how computers interact is the basis for many practical programs. In this feature, I allowed for connection to a server, then connnect a local host (using a loopbak address) to said server by using the Three-Way-Handshake.

## How to test this program

There are 3 different workloads for this assignment:

- `assign4-small-workload.txt` - 15 files (up 350k bytes), 400k+ operations
  - This took about 46 seconds on high-end server.

- `assign4-medium-workload.txt` - 159 files (up to 400k bytes), 4M operations
  - This took about 11 min, 38 seconds on high end server

- `assign4-jumbo-workload.txt` - 250 files (up to 1.5M bytes), 6M+ operations
  - This took about 18 minutes, 53 seconds on high end server

**Note:** logs may get very large, you may want to either disable them, delete them between runs, or increase disk space. Similarly, you may want to increase the resources allocated(more CPU cores, RAM, and disk) to speed up the simulation run if things are too slow. 

## How to compile and test

- To cleanup the compiled objects, run:
  ```
  make clean
  ```

- To compile the code, you have to use:
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

- To run the client(on a seperate console):
  ```
  ./fs3_client -v -l fs3_client_log_small.txt assign4-small-workload.txt
  ./fs3_client -v -l fs3_client_log_medium.txt assign4-medium-workload.txt
  ./fs3_client -v -l fs3_client_log_jumbo.txt assign4-jumbo-workload.txt
  ```

- If the program completes successfully, the following should be displayed as the last log entry:
  ```
  FS3 simulation: all tests successful!!!
  ```
  
## Thanks!
Thank you for checking out my first large scale project! This was a very fun and insightful project that has improved my coding skills and knowledge to an entirely new level. I look forward to developing new programs in the near future!
