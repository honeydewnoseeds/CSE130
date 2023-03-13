// Asgn 2: A simple HTTP server.
// By: Eugene Chou
//     Andrew Quinn
//     Brian Zhao

#include "asgn2_helper_funcs.h"
#include "connection.h"
#include "debug.h"
#include "response.h"
#include "request.h"

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

void handle_connection(int);

void handle_get(conn_t *);
void handle_put(conn_t *);
void handle_unsupported(conn_t *);

int main(int argc, char **argv) {
    // verify the format of the arguments
    if (argc < 2) {
        warnx("wrong arguments: %s port_num", argv[0]);
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // checking port number
    char *endptr = NULL;
    size_t port = (size_t) strtoull(argv[1], &endptr, 10);
    if (endptr && *endptr != '\0') {
        warnx("invalid port number: %s", argv[1]);
        return EXIT_FAILURE;
    }

    // initializing sockets for port
    signal(SIGPIPE, SIG_IGN);
    Listener_Socket sock;
    listener_init(&sock, port);

    // intializing errno
    errno = 0;

    // Listener
    while (1) {
        int connfd = listener_accept(&sock);
        handle_connection(connfd);
        close(connfd);
    }

    return EXIT_SUCCESS;
}

void handle_connection(int connfd) {

    // creates a new connection
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
}

void handle_get(conn_t *conn) {

    // retrieves the URI
    char *uri = conn_get_uri(conn);
    //debug("GET request not implemented. But, we want to get %s", uri);

    // What are the steps in here?

    // 1. Open the file.
    int file_fd = open(uri, O_RDONLY);
    const Response_t *res = NULL;
    // If  open it returns < 0, then use the result appropriately
    //   a. Cannot access -- use RESPONSE_FORBIDDEN
    //   b. Cannot find the file -- use RESPONSE_NOT_FOUND
    //   c. other error? -- use RESPONSE_INTERNAL_SERVER_ERROR
    // (hint: check errno for these cases)!
    if (file_fd < 0) {
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
        return;
    }

    // 2. Get the size of the file.
    // (hint: checkout the function fstat)!
    struct stat buffer;
    fstat(file_fd, &buffer);
    uint64_t size = buffer.st_size;

    // 3. Check if the file is a directory, because directories *will*
    // open, but are not valid.
    // (hint: checkout the macro "S_IFDIR", which you can use after you call fstat!)
    if (S_ISDIR(buffer.st_mode)) {
        fprintf(stdout, "it's a directory!\n");
        res = &RESPONSE_FORBIDDEN;
        conn_send_response(conn, res);
        return;
    }

    // 4. Send the file
    // (hint: checkout the conn_send_file function!)
    res = &RESPONSE_OK;
    conn_send_file(conn, file_fd, size);
    close(file_fd);
}

void handle_unsupported(conn_t *conn) {
    debug("handling unsupported request");

    // send responses
    conn_send_response(conn, &RESPONSE_NOT_IMPLEMENTED);
}

void handle_put(conn_t *conn) {

    char *uri = conn_get_uri(conn);
    const Response_t *res = NULL;
    debug("handling put request for %s", uri);

    // Check if file already exists before opening it.
    bool existed = access(uri, F_OK) == 0;
    debug("%s existed? %d", uri, existed);

    // Open the file..
    int fd = open(uri, O_CREAT | O_TRUNC | O_WRONLY, 0600);
    if (fd < 0) {
        debug("%s: %d", uri, errno);
        if (errno == EACCES || errno == EISDIR || errno == ENOENT) {
            res = &RESPONSE_FORBIDDEN;
            goto out;
        } else {
            res = &RESPONSE_INTERNAL_SERVER_ERROR;
            goto out;
        }
    }

    res = conn_recv_file(conn, fd);

    if (res == NULL && existed) {
        res = &RESPONSE_OK;
    } else if (res == NULL && !existed) {
        res = &RESPONSE_CREATED;
    }

    close(fd);

out:
    conn_send_response(conn, res);
}
