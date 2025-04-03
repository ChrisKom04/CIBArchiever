#include "ADTVector.h"
#include <stdbool.h>
#include <stdint.h>

/*Input Flags*/
#define C 1
#define A 2
#define J 4
#define D 8
#define Q 16

#define X 32
#define M 64
#define P 128

typedef struct cib_arguments{
    Vector paths;

    char *cib_file;
    uint8_t flags;
}* CIBArgs;

/*Reads the arguments and stores them inside a cib_arguments struct.

If the input was correct a pointer to the cib_arguments struct is returned.
Otherwise, NULL is returned.*/
CIBArgs CIBReadArgs(int argc, char *argv[]);

/*Frees the memory of cib_arguments struct.*/
void CIBArgsDestroy(CIBArgs args);

/*Given a vector of paths, which may be full paths or relative to cwd, this function
makes them relative to the given base_dir.

The given vector remains intact. Returns a vector containing the relative paths.*/
Vector CreateRelativePath(Vector paths, char *base_dir);

/*Error Message: Cannot Delete ".".*/
void CIBCannotDeleteRoot();

/*Error Message: Path not found inside .cib file.*/
void CIBPathNotFound(char *path, char *cib_file);

/*Error Message: Path cannot be inserted.*/
void CIBCannotInsertPath(char *path);

/*Error Message: Path cannot be compressed.*/
void CIBCannotCompress(char *path);

/*Error Message: Path does not exist.*/
void CIBPathDoesNotExist(char *path);

/*Error Message: Path is not a directory.*/
void CIBPathIsNotDir(char *path);