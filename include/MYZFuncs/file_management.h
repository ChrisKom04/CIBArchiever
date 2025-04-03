#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>

#include "ADTVector.h"

/*Opens the existing cib file defined by path. If the opening is successful, the file is
mapped and the pointers are set to pointing to the different partitions of the file.

On success 0 is returned. On failure, -1 is returned.*/
int OpenExistingCIB(char *path);

/*Closes the open cib file with file descriptor the global int fd and unmaps it.*/
void CloseExistingCIB();

/*Sets the size of the different partitions of the cib file to the given sizes. After truncation, the
file is re-mapped and the pointers of the different partitions are recalculated.

The header's information about the size of the different partitions is updated.
Warning! This function does not shift chunks of data inside the cib file.

On success 0 is returned. On failure -1 is returned.*/
int TruncMapAndUpdate(uint64_t header_size, int64_t md_size, int64_t data_size, bool mapped);

/*Calculates the space needed to store the paths inside the given vector.*/
uint64_t CalculateSpace(Vector paths, uint32_t *node_blocks, uint64_t *data_size);

/*Creates the directory specified by path. Make sure that its parent directory exists before
calling this function. If you 're not sure call CreateDirRec() instead.

On success 0 is returned. On failure an error message is printed and -1 is returned.*/
int CreateDir(char *path);

/*Given path of directories, e.g. dirA/dirB/dirC this function creates every directory of the path.
In the case described it will call CreateDir() on dirA, dirA/dirB and dirA/dirB/dirC.

On success 0 is returned. On failure -1 is returned.*/
int CreateDirRec(char *path);