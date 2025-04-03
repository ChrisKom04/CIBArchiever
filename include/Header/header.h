#include <stdint.h>

/*Returns the base directory.*/
char *HeadGetBaseDir();

/*Calculates and returns the space that the header needs.*/
uint64_t HeadCalculateNeededSpace(char *base_dir);

/*Initialize the header partition. Base_dir will be the base directory
for the .cib file.*/
void HeadInit(char *base_dir);

/*Returns the metadata size.*/
uint64_t HeadGetMDSize();

/*Set the metadata size to the desired value.*/
void HeadSetMDSize(uint64_t size);

/*Returns the data size.*/
uint64_t HeadGetDataSize();

/*Set the data size to the desired value.*/
void HeadSetDataSize(uint64_t size);

/*Returns the header size.*/
uint64_t HeadGetHeaderSize();

/*Returns the file's total size.*/
uint64_t HeadGetFileSize();

/*Returns the metadata's CIBList blocks.*/
uint32_t HeadGetListBlocks();

/*Sets the metadata's CIBList blocks count to the given value.*/
void HeadSetListBlocks(uint32_t blocks);

/*Returns the number of entries that the metadata partition holds.*/
uint64_t HeadGetListEntries();

/*Sets the metadata CIBList entries count to the given value.*/
void HeadSetListEntries(uint64_t entries);

/*Returns the capacity of the CIBList in entries.*/
uint64_t HeadGetListCapacity();

/*Returns the nest level of the CIBList.*/
uint8_t HeadGetNestLevel();

/*Sets the CIBList nest level to the given value.*/
void HeadSetNestLevel(uint8_t nest_level);

/*Returns the number of free node blocks in metadata partition.*/
uint32_t HeadGetMDFreeNodeBlocks();

/*Sets the counter of free node blocks in metadata partition to the given value.*/
void HeadSetMDFreeNodeBlocks(uint32_t blocks);