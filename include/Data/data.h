#include <stdint.h>
#include <stdbool.h>

#define FILE_EXTRA_DATA 32

#define DATA_BLOCK_SIZE 1024
#define DATA_BLOCK_SHIFT 10

#define EXTRA_BLOCKS_NEEDED 1

typedef uint64_t DataBlockId;

/*Inserts the data of the file defined by the given path inside the data "partition".
If zipped is true then data are marked as zipped.*/
DataBlockId DataInsertFile(char *path, bool zipped);

/*Inserts the stored path of the symlink, which is defined by path, in data partition.*/
DataBlockId DataInsertLink(char *path);

/*Deletes the file which is stored in data partition starting from the given block.*/
void DataDeleteFile(DataBlockId block);

/*Calculates the amount of blocks needed to store the given bytes of data.*/
uint64_t DataCaclulateNeededBlocks(uint64_t size);

/*Initializes the data partition. Minimum 1 block needed.
Marks rest of the "blocks-1" blocks as a free chunk.*/
void DataInit(uint64_t blocks);

/*With this function the "blocks" last blocks of the data partition form a chunk 
which is considered a free chunk.

Useful when truncating in order to add the newly created blocks in the data partition.*/
void DataInsertFreeBlocks(uint64_t blocks);

/*Scans the struct that holds the free chunks to find the chunk whose position is at the end of the
data "partition". If found then we shrink the data partition and shift the metadata partition, thus 
reducing the file's size.*/
void DataRemoveLastChunk();

/*Extracts the file that is stored in the data chunk whose first block is "block" inside
the file defined by path. The file is opened/created with the given permissions.

If the file was zipped then fork and exec are called to unzip the file using "gunzip".
This function will not use wait() to collect the zombie process created.

Returns true if the file was zipped or false if it wasn't.*/
bool DataExtractFile(DataBlockId block, char *path, int perm);

/*Extracts the link that is stored in the data chunk whose first block is "block" in the link
specified by the given path.*/
void DataExtractLink(DataBlockId block, char *path);