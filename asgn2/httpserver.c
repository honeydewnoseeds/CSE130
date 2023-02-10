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

#define MAX_REQUEST  2048
#define MAX_BUF      4096
#define MAX_RESPONSE 200 // since the longest resonse without message could have max 185 bytes, using 200 just to be careful
#define MAX_PHRASE 22 // since the longest status phrase has 22 bytes
////// Request Line Regex
#define METHOD       "([a-zA-Z]{1,8})" // character range [a-zA-Z] at most 8 characters
#define URI          "([a-zA-Z0-9.-]{1,63})" // characte range [a-zA-Z0-9.-] and at least 2 characters and at most 64 characters
#define VERSIONX     "([0-9]{1})" // HTTP/VERSIONX.VERSIONY
#define VERSIONY     "([0-9]{1})"
#define REQUEST_LINE "^" METHOD " " "/" URI " HTTP/" VERSIONX "." VERSIONY

////// Header-field Regex
#define KEY          "([a-zA-Z0-9.-]{1,128})" // character range words, numbers, . and -
#define VALUE        "([ -~]{1,128})" // any ascii character
#define COLON        "([:]{1})"
#define HEADER_FIELD "^" KEY COLON VALUE

static int Status_Code = 999;
static char *found = NULL;
static int Length = 999;

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
    Something_Wrong = 999,
};

static const char *const StatusPhrase[] = { [Ok] = "OK",
    [Created] = "Created",
    [Bad_Request] = "Bad Request",
    [Forbidden] = "Forbidden",
    [Not_Found] = "Not Found",
    [Internal_Server_Error] = "Internal Server Error",
    [Not_Implemented] = "Not Implemented",
    [Version_Not_Supported] = "Version Not Supported",
    [Something_Wrong] = "Weird" };

// @param socket_fd: the socket we're writting to
// @param file_fd: the file we're reading from
// @return: nothing at the moment
// @usage: get retrieves data in filename and output it to socket_fd
// closes filename when exit, but not socket_fd
void get(int file_fd, int socket_fd) {
    char buff[MAX_BUF] = { 0 };
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
    } while (bytes_read > 0);
}

// @param filename: the name of the file we're reading from
// @param socket_fd: the socket we're reading from
// @return: nothing at the moment
// @usage: put what we read from socket_fd into filename
// closes filename when exit, but not socket_fd
void put(int file_fd, int socket_fd, int bytes) {
    char buff[MAX_BUF] = { 0 };
    int bytes_wrote;
    int n;
    do {
        // clear buff
        bzero(buff, MAX_BUF);
        n = read_until(socket_fd, buff, bytes, NULL);
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
void sending_message(int socket_fd, int vx, int vy, int code, int length) {
    char buffer[MAX_RESPONSE] = { 0 };
    int n;
    if (code == 200) {
        n = sprintf(buffer, "HTTP/%d.%d %d %.*s\r\nContent-Length: %d\r\n\r\n", vx, vy, code,
            length, StatusPhrase[code], length);
    } else {
        n = sprintf(buffer, "HTTP/%d.%d %d %.*s\r\nContent-Length: %d\r\n\r\n%.*s\n", vx, vy, code,
            MAX_PHRASE, StatusPhrase[code], length, MAX_PHRASE, StatusPhrase[code]);
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
    } else {
        return true;
    }
}

// @param buf: the buffer to look for pattern from regex r
// @param r: the regex we want to check on
// @usage: used to check if buf contains invalid format
// @return: returns NULL when everything matches with regex r
// @return: returns NULL and status_code set to 400 when something doesn't match
// @return: returns a string when everything matches except the last token
// because the last token might be incomplete due to read_bytes restrictions
char *parsing_header(char *buf, regex_t r) {
    int rn;
    char *Ntoken = NULL;
    char *Ctoken = NULL;

    Ntoken = strtok(buf, "\r\n");
    while (Ntoken != NULL) {
        // update current token
        Ctoken = Ntoken;
        // regex current token
        rn = regexec(&r, Ctoken, 0, NULL, 0);
        // get the next token
        Ntoken = strtok(NULL, "\r\n");
        // when regex doens't match and we're not at the end of the buf
        if (rn == 1 && Ntoken != NULL) {
            Status_Code = 400;
            Ntoken = NULL;
            return Ntoken;
            // when regex doesn't match and we are at the end of the buf
            // returning Ctoken because the rest of the header field might
            // be cut off due to size read from read_until
        } else if (rn == 1) {
            return Ntoken;
            // when the regex matches
        } else {
            // look for content-Length
            found = strstr(Ctoken, "Content-Length");
            if (found) {
                sscanf(found, "%*[^0123456789]%d", &Length);
            } else {
                found = strstr(Ctoken, "content-length");
                if (found) {
                    sscanf(found, "%*[^0123456789]%d", &Length);
                }
            }
        }
    }
    // when the whole buf matches with the regex requriemend
    Ctoken = NULL;
    return Ctoken;
}

int main(int argc, char **argv) {
    Listener_Socket socket_fd;
    //struct sockaddr_in server_addr;
    int port;
    errno = 0;
    char buf[MAX_REQUEST] = { 0 };
    //int Status_Code;
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
        char buf_rest[MAX_BUF] = { 0 };
        strncpy(buf_rest, buf + len + 2, MAX_REQUEST - len);

        buf[len] = 0;

        // creating new regular expression for request_line
        regex_t req;
        int req_r;
        regmatch_t req_line[5];

        req_r = regcomp(&req, REQUEST_LINE, REG_EXTENDED);
        if (req_r) {
            Status_Code = 500;
            sending_message(new_socket_fd, 1, 1, Status_Code, 22);
            close(new_socket_fd);
            continue;
        }

        // check if parsed buffer matches the Method URI Version format
        req_r = regexec(&req, buf, 5, req_line, 0);
        // no matches found, should return status 400
        if (req_r) {
            //printf("No matches found\n");
            regfree(&req);
            Status_Code = 400;
            sending_message(new_socket_fd, 1, 1, Status_Code, 12);
            close(new_socket_fd);
            continue;
        }

        // retrieving method from request line
        char method[8] = { 0 };
        int method_start = req_line[1].rm_so;
        int method_end = req_line[1].rm_eo;
        int method_length = method_end - method_start;
        strncpy(method, buf + method_start, method_length);
        // retrieving uri from request line
        char uri[63] = { 0 };
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

        if (!(VerX == 1 && VerY == 1)) {
            if ((0 <= VerX) && (0 <= VerY) && (VerX <= 9) && (VerY <= 9)) {
                Status_Code = 505;
                sending_message(new_socket_fd, 1, 1, Status_Code, 22);
                close(new_socket_fd);
                continue;
            } else {
                Status_Code = 400;
                sending_message(new_socket_fd, 1, 1, Status_Code, 12);
                close(new_socket_fd);
                close(file_fd);
                continue;
            }
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
                } else {
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
                    close(file_fd);
                    close(new_socket_fd);
                    continue;
                }
            }

            Status_Code = 200;
            struct stat st;
            fstat(file_fd, &st);
            sending_message(new_socket_fd, 1, 1, Status_Code, st.st_size);
            get(file_fd, new_socket_fd);
            close(file_fd);
            close(new_socket_fd);
        } else if (strcmp(method, "PUT") == 0) {

            file_fd = open(uri, O_WRONLY | O_TRUNC);

            // checks if we can't access the file
            if (file_fd < 0) {
                if (errno == EROFS) {
                    Status_Code = 403;
                    sending_message(new_socket_fd, 1, 1, Status_Code, 10);
                    close(new_socket_fd);
                    continue;
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
                Status_Code = 201;
            } else {
                Status_Code = 200;
            }

            //////////// Parsing for Header-Field //////////////////

            regex_t head;
            int head_r;
            // compiling regex for header-field
            head_r = regcomp(&head, HEADER_FIELD, REG_EXTENDED);
            if (head_r) {
                Status_Code = 500;
                sending_message(new_socket_fd, 1, 1, Status_Code, 22);
                close(file_fd);
                close(new_socket_fd);
                continue;
            }

            // see if header exist
            header = check_header(buf_rest);
            char buf_rest_rest[MAX_BUF] = { 0 };
            char temp[MAX_BUF] = { 0 };
            char *t = NULL; // temp
            // loopinggg
            while (!header) {
                // parsing through buf_rest
                // and store the result into buf_rest_rest
                t = parsing_header(buf_rest, head);
                // if the parsing actuall had somethint left
                if (t) {
                    strcpy(buf_rest_rest, t);
                    bzero(temp, MAX_BUF);
                    read_until(new_socket_fd, temp, MAX_BUF - strlen(buf_rest_rest), "\r\n\r\n");
                    bzero(buf_rest, MAX_BUF);
                    sprintf(buf_rest, "%.*s%.*s", (int) strlen(buf_rest_rest), buf_rest_rest,
                        (int) strlen(temp), temp);
                } else if (Status_Code == 400) {
                    break;

                } else {
                    read_until(new_socket_fd, buf_rest, MAX_BUF, "\r\n\r\n");
                }
                header = check_header(buf_rest);
                bzero(buf_rest_rest, MAX_BUF);
            } // end of big while loop

            // this means the status_code was 400 during last while loop
            if (!header) {
                sending_message(new_socket_fd, 1, 1, Status_Code, 12);
                close(new_socket_fd);
                close(file_fd);
                continue;
            }

            // out of the loop means header is found
            // and the rest of the input is stored in buf_rest

            // seperate header-field in buf_rest
            char header_buf[MAX_BUF] = { 0 };
            t = strstr(buf_rest, "\r\n\r\n");
            sprintf(header_buf, "%.*s", (int) (strlen(buf_rest) - strlen(t)), buf_rest);

            // parse through header_buf to check with regex
            t = parsing_header(header_buf, head);

            if (Length == 999) {
                Status_Code = 400;
                sending_message(new_socket_fd, 1, 1, Status_Code, 12);
                close(new_socket_fd);
                close(file_fd);
                continue;
            }
            if (Status_Code == 400) {
                sending_message(new_socket_fd, 1, 1, Status_Code, 12);
                close(new_socket_fd);
                close(file_fd);
                continue;
            }
            // seperate remaining message form buf_rest
            t = strtok(buf_rest, "\r\n\r\n");
            t = strtok(NULL, "\r\n\r\n");
            if (t) {

                int bytes_wrote = write_all(file_fd, t, strlen(t));
                if (bytes_wrote < 0 || errno != 0) {
                    fprintf(stderr, "%s\n", strerror(errno));
                    fprintf(stderr, "can't write to socket\n");
                    Status_Code = 500;
                    sending_message(new_socket_fd, 1, 1, Status_Code, 22);
                    close(new_socket_fd);
                    continue;
                }

                put(file_fd, new_socket_fd, Length - strlen(t));
                sending_message(
                    new_socket_fd, 1, 1, Status_Code, strlen(StatusPhrase[Status_Code]));
            } else {
                put(file_fd, new_socket_fd, Length);
                sending_message(
                    new_socket_fd, 1, 1, Status_Code, strlen(StatusPhrase[Status_Code]));
            }
            close(file_fd);
            close(new_socket_fd);

            // when the command is something other than GET or PUT
        } else {
            Status_Code = 501;
            sending_message(new_socket_fd, 1, 1, Status_Code, 16);
            close(new_socket_fd);
        }
    }

    return 0;
}
