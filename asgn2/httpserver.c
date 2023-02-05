#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "asgn2_helper_funcs.h"

#define MAX_BUF 4096

int main(int argc, char **argv) {
    Listener_Socket socket_fd;
    //struct sockaddr_in server_addr;
    int port;
    char buf[MAX_BUF];
    
    // checking usage
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    // getting port number
    port = atoi(argv[1]);
        // if port if not between 1 and 65535
    if (port < 1 || 65535 < port){
        fprintf(stderr, "Invalid Port\n");
        return 1;
    }    
    
    // bounds socket_fd to localhost and listen on port
    if (listener_init(&socket_fd, port) < 0) {
        fprintf(stderr, "Invalid Port\n");
        return 1;
    }
    
    // the server runs forever until terminated by ctrl-c
    while(1) {
        int new_socket_fd;
        // Blocks until a new client connection
        new_socket_fd = listener_accept(&socket_fd);
        bzero(buf, MAX_BUF);
        int n = read_until(new_socket_fd, buf, MAX_BUF, "poop");
        if (n < 0) {
            fprintf(stderr, "can't read from socket\n");
            return 1;
        }
        fprintf(stdout, "%s\n", buf);
        close(new_socket_fd);

    }
	return 0;
}
