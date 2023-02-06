#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include "asgn2_helper_funcs.h"

#define MAX_BUF 4096

// @param filename: the name of the file we're reading from
// @param socket_fd: the socket we're writting to
// @return: nothing at the moment
// get retrieves data in filename and output it to socket_fd
// closes filename when exit, but not socket_fd
void get(char *filename, int socket_fd) {
    char buff[MAX_BUF];
    int n = 0;
    int file_fd = open(filename, O_RDONLY);

    // status code 403
    if (file_fd < 0) {
        fprintf(stderr, "Invalid Command\n");
        exit(1);
    }

    int bytes_read = 0;

    // since read_until terminates when buf contains NULL or reaches EOF
    // or MAX_BUF bytes is read
    bytes_read = read_until(file_fd, buff, MAX_BUF, NULL);
    if (bytes_read < 0 || errno != 0) {
        fprintf(stderr, "%s\n", strerror(errno));
        fprintf(stderr, "can't read from socket\n");
        exit(1);
    }

    n = write_all(socket_fd, buff, strlen(buff));
    if (n < 0 || errno != 0) {
        fprintf(stderr, "%s\n", strerror(errno));
        fprintf(stderr, "can't write to socket\n");
        exit(1);
    }
    close(file_fd);
}

// @param filename: the name of the file we're reading from
// @param socket_fd: the socket we're reading from
// @return: nothing at the moment
// put what we read from socket_fd into filename
// closes filename when exit, but not socket_fd
void put(char *filename, int socket_fd) {
    char buff[MAX_BUF];
    int file_fd;
    int bytes_wrote;
    int n;

    // open file as: if filename doesn't exist, create a file and user have R,W,E permission
    // if exist, write only, and non_empty then truncate file to length 0
    // if exist and empty, then write only
    file_fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);
    do {
        // clear buff
        bzero(buff, MAX_BUF);
        n = read_until(socket_fd, buff, MAX_BUF, NULL);
        if (n < 0 || errno != 0) {
            fprintf(stderr, "%s\n", strerror(errno));
            fprintf(stderr, "can't read from socket\n");
            exit(1);
        }

        bytes_wrote = write_all(file_fd, buff, n);
        if (bytes_wrote < 0 || errno != 0) {
            fprintf(stderr, "%s\n", strerror(errno));
            fprintf(stderr, "can't write to socket\n");
            exit(1);
        }

    } while (n > 0);
    close(file_fd);
}

int main(int argc, char **argv) {
    Listener_Socket socket_fd;
    //struct sockaddr_in server_addr;
    int port;
    errno = 0;
    char buf[MAX_BUF];

    // checking usage
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    // getting port number
    port = atoi(argv[1]);
    // if port if not between 1 and 65535
    if (port < 1 || 65535 < port) {
        fprintf(stderr, "Invalid Port\n");
        return 1;
    }

    // bounds socket_fd to localhost and listen on port
    if (listener_init(&socket_fd, port) < 0) {
        fprintf(stderr, "Invalid Port\n");
        return 1;
    }

    // the server runs forever until terminated by ctrl-c
    while (1) {
        int new_socket_fd;
        // Blocks until a new client connection
        new_socket_fd = listener_accept(&socket_fd);
        bzero(buf, MAX_BUF);

        /*
        // reading from client
        int n;
        n = read_until(new_socket_fd, buf, MAX_BUF, NULL);
        if (n < 0 || errno != 0) {
            fprintf(stderr, "%s\n", strerror(errno));
            fprintf(stderr, "can't read from socket\n");
            return 1;
        } */

        /*
        ///////////// GET ////////////////////////////
        //get("medium_ascii.txt", new_socket_fd);  

        //////////// PUT /////////////////////////////
        int file_fd;
        int bytes_wrote;
        file_fd = open("test.txt", O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);
        
        do{
            bytes_wrote = write_all(file_fd, buf, n);
            if (bytes_wrote < 0 || errno != 0) {
                fprintf(stderr, "%s\n", strerror(errno));
                fprintf(stderr, "can't write to socket\n");
                return 1;
            }

            bzero(buf, MAX_BUF);
            n = read_until(new_socket_fd, buf, MAX_BUF, NULL);
            if (n < 0 || errno != 0) {
                fprintf(stderr, "%s\n", strerror(errno));
                fprintf(stderr, "can't read from socket\n");
                return 1;
            }
        }while(n > 0);
        
        close(file_fd); */
        put("test.txt", new_socket_fd);
        close(new_socket_fd);
    }
    return 0;
}
