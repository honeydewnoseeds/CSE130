// Asgn 2: A simple HTTP server.
// By: Eugene Chou
//     Andrew Quinn
//     Brian Zhao

#include "asgn2_helper_funcs.h"
#include "connection.h"
#include "debug.h"
#include "response.h"
#include "request.h"
#include "queue.h"

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/file.h>
#include <sys/stat.h>

#define OPTIONS "t:"

queue_t *q = NULL;
pthread_mutex_t mutex;

void *handle_connection();

void handle_get(conn_t *);
void handle_put(conn_t *);
void handle_unsupported(conn_t *);

void audit(conn_t *conn, const Response_t *res) {

    const Request_t *req = conn_get_request(conn);
    char *URI = conn_get_uri(conn);
    const char *oper = request_get_str(req);
    uint16_t code = response_get_code(res);
    char *id = conn_get_header(conn, "Request-Id");

    fprintf(stderr, "%s,%s,%hu,%s\n", oper, URI, code, id);
}

int main(int argc, char **argv) {
    // verify the format of the arguments
    if (argc < 2) {
        warnx("wrong arguments: %s port_num", argv[0]);
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // default number of worker thread is 4
    int num_thread = 4;
    int opt;
    while ((opt = getopt(argc, argv, OPTIONS)) != -1) {
        switch (opt) {
        case 't': num_thread = strtoul(optarg, NULL, 10); break;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Expected argument after options\n");
        return EXIT_FAILURE;
    }

    size_t port = (size_t) strtoull(argv[optind], NULL, 10);

    // initializing sockets for port
    signal(SIGPIPE, SIG_IGN);
    Listener_Socket sock;
    listener_init(&sock, port);

    // intializing errno
    errno = 0;

    // new queue
    // size = num_thread
    q = queue_new(num_thread);

    // an array of threads with size = size of threads indicated
    pthread_t threads[num_thread];
    // initializing each worker thread
    for (int i = 0; i < num_thread; i++) {
        pthread_create(&threads[i], NULL, handle_connection, NULL);
    }

    // initializing mutex for flock
    int rc = 0;
    rc = pthread_mutex_init(&mutex, NULL);
    assert(!rc);

    // Listener
    while (1) {
        uintptr_t connfd = listener_accept(&sock);
        queue_push(q, (void *) connfd);
        //close(connfd);
    }

    return EXIT_SUCCESS;
}

void *handle_connection() {
    // worker thread
    while (1) {
        // creates a new connection
        uintptr_t connfd;
        // pops from q to get connfd
        // if q empty, worker thread get block
        queue_pop(q, (void **) &connfd);
        // creating new connection
        conn_t *conn = conn_new(connfd);

        // res is NULL when data from client is correctly formated
        // else res points to a response that should be sent to client
        const Response_t *res = conn_parse(conn);

        // if the message is ill-formatted
        if (res != NULL) {
            conn_send_response(conn, res);
            // if the message is correctly formatted
        } else {
            // not sure what this does
            debug("%s", conn_str(conn));
            // returns request from parsing data from connections
            const Request_t *req = conn_get_request(conn);
            // if request is get
            if (req == &REQUEST_GET) {
                handle_get(conn);
                // else if the requst is put
            } else if (req == &REQUEST_PUT) {
                handle_put(conn);
                // else the request is unsupported
            } else {
                handle_unsupported(conn);
            }
        }
        conn_delete(&conn);
        close(connfd);
    }
    return NULL;
}

void handle_get(conn_t *conn) {
    // retrieves the URI
    char *uri = conn_get_uri(conn);
    //debug("GET request not implemented. But, we want to get %s", uri);

    // What are the steps in here?

    // 1. Open the file.
    // lock
    pthread_mutex_lock(&mutex);
    int file_fd = open(uri, O_RDONLY);
    const Response_t *res = NULL;
    // If  open it returns < 0, then use the result appropriately
    //   a. Cannot access -- use RESPONSE_FORBIDDEN
    //   b. Cannot find the file -- use RESPONSE_NOT_FOUND
    //   c. other error? -- use RESPONSE_INTERNAL_SERVER_ERROR
    // (hint: check errno for these cases)!
    if (file_fd < 0) {
        pthread_mutex_unlock(&mutex);
        debug("%s: %d", uri, errno);
        if (errno == EACCES) {
            res = &RESPONSE_FORBIDDEN;
        } else if (errno == ENOENT) {
            res = &RESPONSE_NOT_FOUND;
        } else {
            // could trigger because it's a directory?
            res = &RESPONSE_INTERNAL_SERVER_ERROR;
        }
        conn_send_response(conn, res);

        // audit
        audit(conn, res);
        return;
    }

    flock(file_fd, LOCK_SH);
    pthread_mutex_unlock(&mutex);

    // 2. Get the size of the file.
    // (hint: checkout the function fstat)!
    struct stat buffer;
    fstat(file_fd, &buffer);
    uint64_t size = buffer.st_size;

    // 3. Check if the file is a directory, because directories *will*
    // open, but are not valid.
    // (hint: checkout the macro "S_IFDIR", which you can use after you call fstat!)
    if (S_ISDIR(buffer.st_mode)) {
        res = &RESPONSE_FORBIDDEN;
        conn_send_response(conn, res);
        close(file_fd);
        audit(conn, res);
        return;
    }

    // 4. Send the file
    // (hint: checkout the conn_send_file function!)
    res = &RESPONSE_OK;
    conn_send_file(conn, file_fd, size);
    //flock(file_fd, LOCK_UN);
    audit(conn, res);
    close(file_fd);
    //audit(conn, res);
}

void handle_unsupported(conn_t *conn) {
    debug("handling unsupported request");

    // send responses
    conn_send_response(conn, &RESPONSE_NOT_IMPLEMENTED);
    audit(conn, &RESPONSE_NOT_IMPLEMENTED);
}

void handle_put(conn_t *conn) {

    char *uri = conn_get_uri(conn);
    const Response_t *res = NULL;
    debug("handling put request for %s", uri);

    // lock
    pthread_mutex_lock(&mutex);
    // Check if file already exists before opening it.
    bool existed = access(uri, F_OK) == 0;
    debug("%s existed? %d", uri, existed);

    // Open the file..
    int fd = open(uri, O_CREAT | O_WRONLY, 0600);
    if (fd < 0) {
        // unlock
        pthread_mutex_unlock(&mutex);
        debug("%s: %d", uri, errno);
        if (errno == EACCES || errno == EISDIR || errno == ENOENT) {
            res = &RESPONSE_FORBIDDEN;
        } else {
            res = &RESPONSE_INTERNAL_SERVER_ERROR;
        }
        conn_send_response(conn, res);
        audit(conn, res);
        return;
    }

    flock(fd, LOCK_EX);
    // unlock
    pthread_mutex_unlock(&mutex);

    // use stat to get the size of the file to truncate
    int t = ftruncate(fd, 0);
    assert(!t);
    //pthread_mutex_unlock(&mutex);
    // write data from the connection to the file fd
    res = conn_recv_file(conn, fd);

    if (res == NULL && existed) {
        res = &RESPONSE_OK;
    } else if (res == NULL && !existed) {
        res = &RESPONSE_CREATED;
    }
    conn_send_response(conn, res);
    audit(conn, res);
    close(fd);
    // audit(conn, res);
}
