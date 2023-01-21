#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>

#define MAX_BUF 4096

int main() {

    char filename[PATH_MAX];
    //char *token;
    char command[MAX_BUF];
    char buffer[MAX_BUF];
    char fn[PATH_MAX];
    char request[MAX_BUF];
    bool commandget = false;
    bool filenameget = false;
    int bt_read = 0;

    // getting "command file.txt" in a very messy way
    do {
        bt_read = read(STDIN_FILENO, buffer, 1);
        if (strcmp(&buffer[0], "\n") == 0) {
            strcpy(filename, fn);
            filenameget = true;
            break;
        } else if (strcmp(&buffer[0], " ") == 0) {
            strcpy(command, request);
            commandget = true;
        } else {
            if (!commandget) {
                strcat(request, buffer);
            } else {
                strcat(fn, buffer);
            }
        }
    } while (bt_read == 1 && !filenameget);

    //////////////////////////// GET //////////////////////////

    if (strcmp(command, "get") == 0) {
        char buf[MAX_BUF];

        if (read(STDIN_FILENO, buf, MAX_BUF) > 0) {
            if (strcmp(buf, "\n") != 0) {
                fprintf(stderr, "Invalid Command\n");
                return 1;
            }
        }
        // code to open filename.
        int fd = open(filename, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "Invalid Command\n");
            return 1;
        }

        // code to read filename and write it to stdout
        int bytes_read = 0;

        // reading while there's something to read
        do {
            bytes_read = read(fd, buf, MAX_BUF);
            if (bytes_read < 0) {
                fprintf(stderr, "Invalid Command\n");
                close(fd);
                return 1;
            } else if (bytes_read > 0) {
                int bytes_written = 0;
                // writing while there's more to write from the buf
                do {
                    int bytes = write(STDOUT_FILENO, buf + bytes_written, bytes_read);
                    if (bytes < 0) {
                        fprintf(stderr, "Cannot write to STDOUT\n");
                        close(fd);
                        return 1;
                    }

                    bytes_written += bytes;
                } while (bytes_written < bytes_read);
            }
        } while (bytes_read > 0);
        close(fd);
    }

    //////////////////////// SET /////////////////////////////////
    else if (strcmp(command, "set") == 0) {
        // code to open file
        int fd = open(filename, O_CREAT | O_WRONLY | O_EXCL, S_IRUSR | S_IWUSR);
        fd = open(filename, O_WRONLY);
        if (fd < 0) {
            fd = open(filename, O_WRONLY);
            if (fd < 0) {
                fprintf(stderr, "Invalid Command\n");
                return 1;
            }
        }

        // code to write to file
        char buff[MAX_BUF];
        int bytes_read = 0;
        do {
            bytes_read = read(STDIN_FILENO, buff, MAX_BUF);
            int bytes_written = 0;
            do {
                int bytes = write(fd, buff + bytes_written, bytes_read);
                if (bytes < 0) {
                    fprintf(stderr, "Cannot write to %s\n", filename);
                    close(fd);
                    return 1;
                }

                bytes_written += bytes;
            } while (bytes_written < bytes_read);
        } while (bytes_read > 1);
        fprintf(stdout, "OK\n");
        close(fd);
    }

    /////////////////////// INVALID COMMAND /////////////////////
    else {
        fprintf(stderr, "Invalid Command\n");
        return 1;
    }

    return 0;
}
