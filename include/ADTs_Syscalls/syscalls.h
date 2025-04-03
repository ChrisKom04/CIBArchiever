
/*Opens the file with name file_name in:
-Read Mode if read == True
-Write Mode if read == False with flags O_CREAT and O_TRUNC in order
            to create the file if it doesnt exist or overwrite it if it exists.

Returns -1 if there were issues and prints a relative message. Return 0 if everything
went fine.

The file desctiptor is stored in *fd.*/
int OpenFile(char *file_name, int *fd, int flags, int perm);

/*Reads "bytes" amount of bytes in a loop from the file with file
descriptor fd.

Returns -1 if there was any issue and a relative message is printed.

Returns the number of bytes read. If the bytes read is less than the requested
that means that it reached EOF before reading all the bytes.*/
int ReadBytes(void *pointer, int bytes, int fd);

/*Write "bytes" amount of bytes from buffer to the file that has file
decriptor fd.

Returns the number of bytes that were written or -1 if there was an error.*/
int WriteBytes(char *buffer, int bytes, int fd);

/*Increases the size of file with file descriptor "fd" by "bytes" amount
of bytes.

Returns 0 if the operation was successful or -1 if there was an error.*/
int IncreaseFileSize(int fd, int bytes);

//Performs dup2 on the two file descriptors and after that closes
//the old file descriptor.
int DupAndClose(int old_fd, int new_fd);