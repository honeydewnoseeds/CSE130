#include <stdbool.h>
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
#include <regex.h>

#include "asgn2_helper_funcs.h"

#define MAX_REQUEST 2048
#define MAX_BUF 4096
#define MAX_RESPONSE 200 // since the longest resonse without message could have max 185 bytes, using 200 just to be careful
#define MAX_PHRASE 22 // since the longest status phrase has 22 bytes

////// Request Line Regex
#define METHOD "([a-zA-Z]{1,8})" // character range [a-zA-Z] at most 8 characters
#define URI "([a-zA-Z][a-zA-Z0-9.-]{1,63})" // characte range [a-zA-Z0-9.-] and at least 2 characters and at most 64 characters
#define VERSIONX "([0-9])" // HTTP/VERSIONX.VERSIONY
#define VERSIONY "([0-9])"
#define REQUEST_LINE "^"METHOD " ""/" URI " HTTP/" VERSIONX "." VERSIONY

////// Header-field Regex
#define HEADER_FIELD "(([a-zA-Z0-9.-]{1,128})([:]{1})([ -~]{1,128}))" 

// global value for status code and phrases
enum StatusCode {
    Ok = 200,
    Created = 201,
    Bad_Request = 400,
    Forbidden = 403,
    Not_Found = 404,
    Internal_Server_Error = 500,
    Not_Implemented = 501,
    Version_Not_Supported = 505,
};

static const char * const StatusPhrase[] = {
    [Ok] = "OK",
    [Created] = "Created",
    [Bad_Request] = "Bad Request",
    [Forbidden] = "Forbidden",
    [Not_Found] = "Not Found",
    [Internal_Server_Error] = "Internal Server Error",
    [Not_Implemented] = "Not Implemented",
    [Version_Not_Supported] = "Version Not Supported"
};

// @param socket_fd: the socket we're writting to
// @param file_fd: the file we're reading from
// @return: nothing at the moment
// @usage: get retrieves data in filename and output it to socket_fd
// closes filename when exit, but not socket_fd
void get(int file_fd, int socket_fd) {
    char buff[MAX_BUF];
    int bytes_read = 0;
    int n;

    // since read_until terminates when buf contains NULL or reaches EOF
    // or MAX_BUF bytes is read
    do {
        bytes_read = read_until(file_fd, buff, MAX_BUF, NULL);
        if (bytes_read < 0 || errno != 0) {
            fprintf(stderr, "%s\n", strerror(errno));
            fprintf(stderr, "can't read from socket\n");
            exit(1);
        }

        n = write_all(socket_fd, buff, bytes_read);
        if (n < 0 || errno != 0) {
            fprintf(stderr, "%s\n", strerror(errno));
            fprintf(stderr, "can't write to socket\n");
            exit(1);
        }
        bzero(buff, MAX_BUF);
    }while(bytes_read > 0);
}

// @param filename: the name of the file we're reading from
// @param socket_fd: the socket we're reading from
// @return: nothing at the moment
// @usage: put what we read from socket_fd into filename
// closes filename when exit, but not socket_fd
void put(int file_fd, int socket_fd) {
    char buff[MAX_BUF];
    int bytes_wrote;
    int n;

    do {
        // clear buff
        bzero(buff, MAX_BUF);
        //fprintf(stdout, "%.*s\n", 4096, buf);
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
}

// @param socket_fd: the socket file descripter we're writing to
// @param vx & vy: the version number HTTP/vx.vy
// @param code: the status code
// @param length: the length of status phrase or the number of bytes in the file if it's a valid GET
// @usage: sends a response to socket_fd
// returns: nothing
void sending_message(int socket_fd, int vx, int vy, int code, int length){
    char buffer[MAX_RESPONSE];
    int n;
    if (code == 200) {
        n = sprintf(buffer, "HTTP/%d.%d %d %.*s\r\nContent-Length: %d\r\n\r\n", vx, vy, code, length, StatusPhrase[code], length);
    }else{
        n = sprintf(buffer, "HTTP/%d.%d %d %.*s\r\nContent-Length: %d\r\n\r\n%.*s\n", vx, vy, code, MAX_PHRASE, StatusPhrase[code], length, MAX_PHRASE, StatusPhrase[code]);
    }
    if (n < 0) {
        fprintf(stderr, "can't write to buffer in sending_message\n");
        exit(1);
    }
    write_all(socket_fd, buffer, n);
}

// @param buf: buffer that might contain the double \r\n
// @usage: check if the buffer contains \r\n\r\n
// @return: true if buffer contain \r\n\r\n else false
bool check_header(char *buf) {
    char *ret = strstr(buf, "\r\n\r\n");
    if (ret == NULL) {
        return false;
    }else {
        return true;
    }
}

int main(int argc, char **argv) {
    Listener_Socket socket_fd;
    //struct sockaddr_in server_addr;
    int port;
    errno = 0;
    char buf[MAX_REQUEST];
    int Status_Code;
    int file_fd;

    bool header = false; // keep tracks if we have header

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
        errno = 0;
        // Blocks until a new client connection
        new_socket_fd = listener_accept(&socket_fd);
        bzero(buf, MAX_REQUEST);

        // getting client input
        int n;
        n = read_until(new_socket_fd, buf, MAX_REQUEST, "\r\n\r\n");
        if (n < 0 || errno != 0) {
        /*
            fprintf(stderr, "%s\n", strerror(errno));
            fprintf(stderr, "cannot read from socket\n");
          */  
            Status_Code = 500;
            sending_message(new_socket_fd, 1, 1, Status_Code, 22);
            close(new_socket_fd);
            continue;
        }

        // checks if we found header
        header = check_header(buf);
        
        //////////////////////////////Parsing Request Line //////////////////////////
        // parsing buf with no \r\n included
        int len = strcspn(buf, "\r\n");
        // saving the extra bytes just incase
        char buf_rest[MAX_REQUEST];
        strncpy(buf_rest, buf + len + 2, MAX_REQUEST - len);

        buf[len] = 0;
                
        // creating new regular expression for request_line
        regex_t req;
        int req_r;
        regmatch_t req_line[5];

        req_r = regcomp(&req, REQUEST_LINE, REG_EXTENDED);
        if (req_r) {
        /*
            char msgbuf[2048];
            regerror(req_r, &req, msgbuf, sizeof(msgbuf));
            fprintf(stderr, "Regex match failed: %s\n", msgbuf);
            fprintf(stderr, "cannot compile regular expressin with REQUEST_LINE\n");
            */
            Status_Code = 500;
            sending_message(new_socket_fd, 1, 1, Status_Code, 22);
            close(new_socket_fd);
            continue;
        }
        
        // check if parsed buffer matches the Method URI Version format
        req_r = regexec(&req, buf, 5, req_line, 0);
            // no matches found, should return status 400
        if (req_r) {
            fprintf(stderr, "No matches found\n");
            regfree(&req);
            Status_Code = 400;
            sending_message(new_socket_fd, 1, 1, Status_Code, 12);
            close(new_socket_fd);
            continue;
        }

        // retrieving method from request line
        char method[8];
        int method_start = req_line[1].rm_so;
        int method_end = req_line[1].rm_eo;
        int method_length = method_end - method_start;
        strncpy(method, buf + method_start, method_length);
        // retrieving uri from request line
        char uri[63];
        int uri_start = req_line[2].rm_so;
        int uri_end = req_line[2].rm_eo;
        int uri_length = uri_end - uri_start;
        strncpy(uri, buf + uri_start, uri_length);
        // retrieving version number x.y
        int VerX, VerY;
        int VerX_start = req_line[3].rm_so;
        VerX = atoi(&buf[VerX_start]);
        int VerY_start = req_line[4].rm_so;
        VerY = atoi(&buf[VerY_start]);

        if (!(VerX == 1) && !(VerY == 1)) {
            Status_Code = 505;
            sending_message(new_socket_fd, 1, 1, Status_Code, 22);
            close(new_socket_fd);
            continue;
        }
        regfree(&req);
        
        /////////////////////////// Processing Request////////////////////////////
   
        if (strcmp(method, "GET") == 0) {
            
            file_fd = open(uri, O_RDONLY);
            // checks why can't we access the file
            if (file_fd < 0) {
                // file doens't exist?
                if (access(uri, F_OK)) {
                    Status_Code = 404;
                    sending_message(new_socket_fd, 1, 1, Status_Code, 10);
                    close(new_socket_fd);
                    continue;
                // we don't have permsision
                }else{
                    Status_Code = 403;
                    sending_message(new_socket_fd, 1, 1, Status_Code, 10);
                    close(new_socket_fd);
                    continue;
                }
            }
            
            while (!header) {
                bzero(buf, MAX_REQUEST);
                n = read_until(new_socket_fd, buf, MAX_REQUEST, "\r\n\r\n");
                header = check_header(buf);
                if (!header && n < MAX_REQUEST) {
                    Status_Code = 400;
                    sending_message(new_socket_fd, 1, 1, Status_Code, 12);
                    close(new_socket_fd);
                    close(file_fd);
                    continue;
                }
            }

            Status_Code = 200;
            struct stat st;
            fstat(file_fd,&st); 
            sending_message(new_socket_fd, 1, 1, Status_Code, st.st_size); 
            get(file_fd, new_socket_fd);
            close(file_fd);
            close(new_socket_fd);
        }else if (strcmp(method, "PUT") == 0) {

            // open file as: if filename doesn't exist, create a file and user have R,W,E permission
            // if exist, write only, and non_empty then truncate file to length 0
            // if exist and empty, then write only
            file_fd = open(uri, O_WRONLY | O_TRUNC);
            
            // checks if we can't access the file
            if (file_fd < 0) {
                if (access(uri, W_OK | X_OK)) {
                    Status_Code = 403;
                    sending_message(new_socket_fd, 1, 1, Status_Code, 10);
                    close(new_socket_fd);
                    continue;
                }
            }
            
            file_fd = open(uri, O_CREAT, S_IRWXU);
            
            // can't create a new file
            if (file_fd < 0) {

             //   fprintf(stderr, "Invalid Command\n");
                Status_Code = 500;
                sending_message(new_socket_fd, 1, 1, Status_Code, 22);
                close(new_socket_fd);
                continue;
            }

            //////////// Parsing for Header-Field //////////////////

           char* token = strtok(buf_rest, "\r\n");
           if (token == NULL) {
               printf("not done yet");
           }else {
               printf("%s\n", token);
           }

            close(file_fd);
            close(new_socket_fd);

        // when the command is something other than GET or PUT
        }else {
            Status_Code = 501;
            sending_message(new_socket_fd, 1, 1, Status_Code, 16);
            close(new_socket_fd);
        
        }
/*

        printf("Method: %.*s\n", (int)sizeof(method), method);
        printf("URI: %.*s\n", (int)sizeof(uri), uri);
 */      

        ///////////// GET ////////////////////////////
        //        get("medium_ascii.txt", new_socket_fd);

        //////////// PUT /////////////////////////////

        //        put("test.txt", new_socket_fd);
        
    }
    return 0;
}
