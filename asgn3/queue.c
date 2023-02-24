#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>

#include "queue.h"

typedef struct queue {
    int length; // number of items in the queue
    int size; // actual compacity of the queue
    int front; // the front of the queue
    int back; // the back of the queue
    void **elem; // the elements
    pthread_mutex_t mutex;
    pthread_cond_t cv_pop;
    pthread_cond_t cv_push;
} queue;

/** @brief Dynamically allocates and initializes a new queue with a
 *         maximum size, size
 *
 *  @param size the maximum size of the queue
 *
 *  @return a pointer to a new queue_t
 */
queue_t *queue_new(int size) {
    queue_t *Q = malloc(sizeof(queue));
    if (Q == NULL) {
        fprintf(stderr, "failed to create new queue in queue_new()\n");
        exit(1);
    }
    Q->elem = (void **) calloc(size, sizeof(void *));
    if (Q->elem == NULL) {
        fprintf(stderr, "failed to allocte for elem in queue_new()\n");
        exit(1);
    }
    int rc;
    rc = pthread_mutex_init(&(Q->mutex), NULL);
    assert(!rc);
    rc = pthread_cond_init(&(Q->cv_pop), NULL);
    assert(!rc);
    rc = pthread_cond_init(&(Q->cv_push), NULL);
    assert(!rc);
    Q->length = 0;
    Q->size = size;
    Q->front = 0;
    Q->back = -1;
    return Q;
}

/** @brief Delete your queue and free all of its memory.
 *
 *  @param q the queue to be deleted.  Note, you should assign the
 *  passed in pointer to NULL when returning (i.e., you should set
 *  *q = NULL after deallocation).
 *
 */
void queue_delete(queue_t **q) {
    if (q != NULL && *q != NULL) {
        int rc;
        rc = pthread_mutex_destroy(&((*q)->mutex));
        assert(!rc);
        rc = pthread_cond_destroy(&((*q)->cv_pop));
        assert(!rc);
        rc = pthread_cond_destroy(&((*q)->cv_push));
        assert(!rc);

        free((*q)->elem);
        free(*q);
        *q = NULL;
    }
}

/** @brief push an element onto a queue
 *
 *  @param q the queue to push an element into.
 *
 *  @param elem th element to add to the queue
 *
 *  @return A bool indicating success or failure.  Note, the function
 *          should succeed unless the q parameter is NULL.
 */
bool queue_push(queue_t *q, void *elem) {
    // if q is NULL
    if (q == NULL || elem == NULL) {
        return false;
    }
    // if the array is full
    pthread_mutex_lock(&(q->mutex));
    while (q->length == q->size) {
        fprintf(stdout, "waiting since queu is full....\n");
        pthread_cond_wait(&(q->cv_pop), &(q->mutex));
    }
    fprintf(stdout, "pushing....\n");
    q->back = ((q->back) + 1) % (q->size);
    q->elem[q->back] = elem;
    q->length++;
    pthread_mutex_unlock(&(q->mutex));
    fprintf(stdout, "done...\n");
    pthread_cond_signal(&(q->cv_push));
    return true;
}

/** @brief pop an element from a queue.
 *
 *  @param q the queue to pop an element from.
 *
 *  @param elem a place to assign the poped element.
 *
 *  @return A bool indicating success or failure.  Note, the function
 *          should succeed unless the q parameter is NULL.
 */
bool queue_pop(queue_t *q, void **elem) {
    if (q == NULL) {
        return false;
    }
    pthread_mutex_lock(&(q->mutex));
    while (q->length == 0) {
        fprintf(stdout, "waiting since queue is empty....\n");
        pthread_cond_wait(&(q->cv_push), &(q->mutex));
    }
    fprintf(stdout, "poping....\n");
    *elem = q->elem[q->front];
    if (elem == NULL) {
        fprintf(stderr, "something's wrong\n");
        exit(1);
    }
    q->front = ((q->front) + 1) % (q->size);
    q->length--;
    pthread_mutex_unlock(&(q->mutex));
    fprintf(stdout, "done...\n");
    pthread_cond_signal(&(q->cv_pop));
    return true;
}
