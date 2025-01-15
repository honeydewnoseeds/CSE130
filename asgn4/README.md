#Assignment 4 directory

This directory contains source code and other files for Assignment 4.

Use this README document to store notes about design, testing, and
questions you have while developing your assignment.

## Building

Build this program into an object file with:
```
$ make
```
Clean up with:
```
$ make clean
```
Format the program with:
```
$ make format
```

## Running

Run this program with:
```
$ ./httpserver [-t num_threads] port
```
The [-t num/_threads] are optional flags to indicate how many worker threads is working in the server. 
Default = 4

## Descriptions

The program acts as a server that process request from clients from port. Similiar structure to assignment 2 httpserver but supports multithreads environment.
The program are based on the structure of thread-pool design, where theres one dispatcher (listener) thread and at least one worker threads. The dispatcher process connections
and pushes the connection file descriptor onto a queue (bounded buffer from assignment 3). The worker thread then pops connections from the queue to start working on them.
Both worker and dispatcher are working indefinately until connection is closed. When each worker thread process a request, a corresponding audit log will be send to stderr
in the format of: 

[Oper],[URI],[Status-Code],[RequestID header value]\n

