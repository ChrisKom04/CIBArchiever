#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "syscalls.h"

//Performs dup2 on the two file descriptors and after that closes
//the old file descriptor.
int DupAndClose(int old_fd, int new_fd){
    if(old_fd != new_fd){
        dup2(old_fd, new_fd);
        close(old_fd);

    }

    return 0;
}

/*Opens the file with the specified name in the mode and with permissions specified by the arguments.

Returns -1 if there were issues and prints a relative message. Returns 0 if everything went fine.

The file desctiptor is stored in *fd.*/
int OpenFile(char *file_name, int *fd, int flags, int perm){
    *fd = open(file_name, flags, perm);

    if(*fd < 0){
        char buff[56 + strlen(file_name)];
        snprintf(buff, sizeof(buff), "\033[1;31mInput Error: \033[0m %s: No such file or directory\n", file_name);

        WriteBytes(buff, strlen(buff), 2);
        return -1;
    }

    return 0;
}

/*Reads "bytes" amount of bytes in a loop from the file with file
descriptor fd.

Returns -1 if there was any issue and a relative message is printed.

Returns the number of bytes read. If the bytes read is less than the requested
that means that it reached EOF before reading all the bytes.*/
int ReadBytes(void *pointer, int bytes, int fd){
    char *p = pointer;
    int bytes_read = 0, size;

    while((size = read(fd, p + bytes_read, bytes - bytes_read)) != 0){
        if(size == -1 && errno == EINTR){
            continue;

        }else if(size == -1){
            perror("Read Error");
            exit(-1);

        }
        

        bytes_read += size;
        if(bytes_read == bytes)
            break;

    }

    return bytes_read;
}

/*Write "bytes" amount of bytes from buffer to the file that has file
decriptor fd.

Returns the number of bytes that were written or -1 if there was an error.*/
int WriteBytes(char *buffer, int bytes, int fd){
    int res, bytes_written = 0;

    while((res = write(fd, buffer + bytes_written, bytes - bytes_written)) != bytes - bytes_written){
        if(errno != EINTR){
            perror("Write Error");
            return -1;

        }else if(res != -1){
            bytes_written += res;
            
        }
    }

    bytes_written += res;
    return bytes_written;
}

/*Increases the size of file with file descriptor "fd" by "bytes" amount
of bytes.

Returns 0 if the operation was successful or -1 if there was an error.*/
int IncreaseFileSize(int fd, int bytes){
    struct stat info;

    if(fstat(fd, &info) == -1){
        perror("fstat: ");
        return -1;

    }

    if(ftruncate(fd, info.st_size + bytes) == -1){
        perror("ftruncate: ");
        return -1;

    }
    
    return 0;
}