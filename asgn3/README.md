#Assignment 3 directory

This directory contains source code and other files for Assignment 3.

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

Run this program with any test files 

## Descriptions

The program acts as a FIFO queue supports two main operations: 

```
queue_push(queue_t *q, void *elem)
```
pushes an elem into queue q. if the queue is full, pthread_cond_wait() would be called until the conditional variable is signaled
mutex is used to prevent threads from entering critical section concurrently
return true if success
return false if failed

```
queue_pop(queue_t *q, void **elem)
```
pops an element from q and stores it to elem. if the queue is empty, pthread_cond_wait() would be called until the conditional variable is signaled
mutex is used to prevent concurrent access to the critical section
return true if success
return false if failed

The program requires two basic operations:

```
queue_new(int size)
```
return a brand new queue with the capacity of size
should be called when creating a brand new queue

```
queue_delete(queue_t **q)
```
returns nothing
should be called when done with the queue to free memories

## Structure

The queue has 8 internal structures

1. The number of elements in the queue
```
int length
```
2. The capacity/ total storage in the queue
```
int size
```
3. The front index of the queue
```
int front
```
4. The back index of the queue
```
int back
```
5. An array of elements
```
void **elem
```
6. Mutex for critical sections
```
pthread_mutex_t mutex
```
7. Conditional Variable to signal when an element is poped from queue
```
pthread_cond_t cv_pop
```
8. Conditional Variable to signal when an element is pushed to queue
```
pthread_cond_t cv_push
```
