#include "header.h"
#include "metadata.h"
#include "syscalls.h"

#include <string.h>
#define max(a,b) ((a) > (b) ? (a) : (b))

typedef struct header{
    uint64_t data_size;         //Data partition size.

    uint64_t md_size;           //Metadata partition size.

    uint64_t list_entries;      //Metadata CIBList entries.
    uint32_t list_blocks;       //Metadata CIBList blocks.
    uint32_t free_node_blocks;  //Metadata free blocks.
    uint8_t nest_level;         //CIBList nest level

    char base_dir[7 + 4096];     //Saves the base_dir path.
}* Header;

extern void *header;

/*Returns the base directory.*/
char *HeadGetBaseDir(){
    return ((Header) header)->base_dir;
}

/*Returns the header size.*/
uint64_t HeadGetHeaderSize(){
    return max(sizeof(struct header), 33 + strlen(((Header) header)->base_dir) + 1);
}

/*Returns the metadata size.*/
uint64_t HeadGetMDSize(){
    return ((Header) header)->md_size;
}

/*Set the metadata size to the desired value.*/
void HeadSetMDSize(uint64_t size){
    ((Header) header)->md_size = size;
    
    return;
}

/*Returns the data size.*/
uint64_t HeadGetDataSize(){
    return ((Header) header)->data_size;
}

/*Set the data size to the desired value.*/
void HeadSetDataSize(uint64_t size){
    ((Header) header)->data_size = size;
    
    return;
}

/*Returns the file's total size.*/
uint64_t HeadGetFileSize(){
    return HeadGetHeaderSize() + HeadGetDataSize() + HeadGetMDSize();
}

/*Returns the metadata's CIBList blocks.*/
uint32_t HeadGetListBlocks(){
    return ((Header) header)->list_blocks;
}

/*Sets the metadata's CIBList blocks count to the given value.*/
void HeadSetListBlocks(uint32_t blocks){
    ((Header) header)->list_blocks = blocks;

    return;
}

/*Returns the number of entries that the metadata partition holds.*/
uint64_t HeadGetListEntries(){
    return ((Header) header)->list_entries;
}

/*Sets the metadata CIBList entries count to the given value.*/
void HeadSetListEntries(uint64_t entries){
    ((Header) header)->list_entries = entries;

    return;
}

/*Returns the capacity of the CIBList in entries.*/
uint64_t HeadGetListCapacity(){
    return HeadGetListBlocks() * LIST_ENTRIES_PER_BLOCK;
}

/*Returns the nest level of the CIBList.*/
uint8_t HeadGetNestLevel(){
    return ((Header) header)->nest_level;
}

/*Sets the CIBList nest level to the given value.*/
void HeadSetNestLevel(uint8_t nest_level){
    ((Header) header)->nest_level = nest_level;

    return;
}

/*Returns the number of free node blocks in metadata partition.*/
uint32_t HeadGetMDFreeNodeBlocks(){
    return ((Header) header)->free_node_blocks;

}

/*Sets the counter of free node blocks in metadata partition to the given value.*/
void HeadSetMDFreeNodeBlocks(uint32_t blocks){
    ((Header) header)->free_node_blocks = blocks;

    return;
}

//------------------------------------------------------

/*Calculates and returns the space that the header needs.*/
uint64_t HeadCalculateNeededSpace(char *base_dir){
    return max(sizeof(struct header), 33 + strlen(base_dir) + 1);
}

/*Initialize the header partition. Base_dir will be the base directory
for the .cib file.*/
void HeadInit(char *base_dir){
    uint64_t header_size = HeadCalculateNeededSpace(base_dir);

    memset(header, 0, header_size);
    strcpy(((Header) header)->base_dir, base_dir);

    return;
}