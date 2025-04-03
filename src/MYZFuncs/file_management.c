#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>

#include "syscalls.h"
#include "ADTVector.h"
#include "cli_utils.h"

#include "data.h"
#include "header.h"

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

extern int fd;
extern void *header;
extern void *data;
extern void *md;

/*Creates the directory specified by path.

On success 0 is returned. On failure an error message is printed and -1 is returned.*/
int CreateDir(char *path){
    if(mkdir(path, 0755) == -1){
        struct stat info; lstat(path, &info);
        
        if(errno == EEXIST && !S_ISDIR(info.st_mode)){
            CIBPathIsNotDir(path);
            return -1;

        }else if(errno != EEXIST){
            perror("mkdir");
            return -1;

        }
    }

    return 0;
}

/*Given path of directories, e.g. dirA/dirB/dirC this function creates every directory of the path.
In the case described it will call CreateDir() on dirA, dirA/dirB and dirA/dirB/dirC.

On success 0 is returned. On failure -1 is returned.*/
int CreateDirRec(char *path){
    char *copy1 = strdup(path);
    char *copy2 = calloc(1, sizeof(char) * (strlen(path) + 1)); strcpy(copy2, ".");

    for(char *token = strtok(copy1, "/"); token != NULL; token = strtok(NULL, "/")){
        strcat(copy2, "/"); strcat(copy2, token);

        if(CreateDir(copy2) == -1)
            return -1;
    }

    return 0;
}

/*Opens the existing cib file defined by path. If the opening is successful, the file is
mapped and the pointers are set to pointing to the different partitions of the file.

On success 0 is returned. On failure, -1 is returned.*/
int OpenExistingCIB(char *path){
    if(OpenFile(path, &fd, O_RDWR, 0777) == -1)
        return -1;

    struct stat info; fstat(fd, &info);

    header = mmap(NULL, info.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(header == MAP_FAILED){
        perror("map");
        return -1;

    }

    data = (char *) header + HeadGetHeaderSize();
    md = (char *) data + HeadGetDataSize();
    
    return 0;
}

/*Closes the open cib file with file descriptor the global int fd and unmaps it.*/
void CloseExistingCIB(){
    struct stat info; fstat(fd, &info);

    munmap(header, info.st_size); close(fd);
    return;
}

/*Sets the size of the different partitions of the cib file to the given sizes. After truncation, the
file is re-mapped and the pointers of the different partitions are recalculated.

The header's information about the size of the different partitions is updated.
Warning! This function does not shift chunks of data inside the cib file.

On success 0 is returned. On failure -1 is returned.*/
int TruncMapAndUpdate(uint64_t header_size, int64_t md_size, int64_t data_size, bool mapped){
    struct stat info; fstat(fd, &info);

    if(mapped == true){
        if(munmap(header, info.st_size) == -1){
            perror("mmap");
            return -1;

        }
    }
    if(ftruncate(fd, header_size + md_size + data_size) == -1){
        perror("ftruncate");
        return -1;

    }

    header = mmap(NULL, header_size + data_size + md_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(header == MAP_FAILED){
        perror("mmap");
        return -1;

    }
    
    data = (char *) header + header_size;
    md = (char *) data + data_size;

    HeadSetMDSize(md_size);
    HeadSetDataSize(data_size);

    return 0;
}

/*Calculates how many data blocks and node blocks are needed to store everything under the directory specified by path.
Returns the number of dirs/entries/lists under the directory.*/
uint64_t CalculateDirSpaceRec(char *path, uint32_t *node_blocks, uint64_t *data_blocks){
    //Obtain stat info about our .cib file.
    struct stat cib_info; fstat(fd, &cib_info);

    //Number of entries under current_directory and subdirectories of current_directory
    uint64_t entries = 0;
    DIR *dir = opendir(path);
    if(dir == NULL){
        perror("opendir");

        exit(-1);
    }

    //Number of entries under current directory
    uint64_t under_dir = 0;

    struct dirent *entry;
    while((entry = readdir(dir)) != NULL){
        char entry_path[strlen(path) + strlen(entry->d_name) + 2];
        snprintf(entry_path, sizeof(entry_path), "%s/%s", path, entry->d_name);

        struct stat info;
        //Something must have gone really wrong. 
        if(lstat(entry_path, &info) == -1){
            perror("lstat");
            
            continue;
        }

        //We will not include the .cib file we are creating inside the .cib file we are creating (hope this makes sense).
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || cib_info.st_ino == info.st_ino)
            continue;


        if(S_ISDIR(info.st_mode)){
            entries += 1 + CalculateDirSpaceRec(entry_path, node_blocks, data_blocks);

        }else if(S_ISLNK(info.st_mode) || S_ISREG(info.st_mode)) {
            entries += 1;
            *data_blocks += DataCaclulateNeededBlocks(info.st_size);

        }

        under_dir++;
    }
    
    *node_blocks += under_dir > 3 ? under_dir / 3 + (under_dir % 3 > 0) : 1;
    
    closedir(dir);
    return entries;
}

/*Calculates the space needed to store the paths inside the given vector.*/
uint64_t CalculateSpace(Vector paths, uint32_t *node_blocks, uint64_t *data_blocks){
    uint64_t entries = 0; uint64_t under_dir_entries = 0;
    *node_blocks = 0;
    *data_blocks = 0;

    for(uint32_t i = 0; i < VectorGetSize(paths); i++){
        char *path = VectorGetAt(paths, i);
        struct stat info;

        if(lstat(path, &info) == -1){
            CIBPathDoesNotExist(path);

            continue;
        }

        if(S_ISDIR(info.st_mode) == true){
            entries += 1 + CalculateDirSpaceRec(path, node_blocks, data_blocks);
        
        }else if(S_ISREG(info.st_mode) || S_ISLNK(info.st_mode)){
            entries += 1;
            *data_blocks += DataCaclulateNeededBlocks(info.st_size);

        }

        under_dir_entries++;
    }

    *node_blocks += (under_dir_entries) / 3 + ((under_dir_entries) % 3 > 0);

    return entries;
}