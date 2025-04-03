#include <stdbool.h>
#include <sys/stat.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>

#include "ADTList.h"

#include "metadata.h"
#include "freelist.h"
#include "cib_struct.h"
#include "header.h"

extern void *md;

//---------------------------------------------------------------
//EntryID-Name Pair Functions

/*A struct that holds Id-Name.*/
typedef struct id_name_pair{
    EntryId entry_id;
    char *name;

}* INPair;

/*Creates an INPair struct in heap and returns a pointer to it.*/
INPair INPairCreate(EntryId entry_id, char *name){
    INPair pair = malloc(sizeof(struct id_name_pair));

    pair->entry_id = entry_id;
    pair->name = strdup(name);

    return pair;
}

/*Destroys the given pair.*/
void INPairDestroy(INPair pair){
    free(pair->name);
    free(pair);

    return;
}

/*Returns the name of the given pair.*/
char *INPairGetName(INPair pair){

    return pair->name;
}

/*Returns the Id of the given pair.*/
EntryId INPairGetId(INPair pair){

    return pair->entry_id;
}

//---------------------------------------------------------------
//General Metadata Functions

//Returns a pointer to the free-list block.
void *GetFreeListBlockAddress(){
    void *new = (void *)((uint64_t) md + ((FREE_LIST_BLOCK) << MD_BLOCK_SHIFT));

    return new;
}

//Returns a pointer to the requested list-block.
void *GetListBlockAddress(uint64_t block_num){
    void *new = (void *)((uint64_t) md + ((CIB_LIST_BLOCK + block_num) << MD_BLOCK_SHIFT));

    return new;
}

//Returns a pointer to the requested node-block.
void *GetNodeBlockAddress(uint64_t block_num){
    void *new = (void *)((uint64_t) md + ((CIB_LIST_BLOCK + block_num + HeadGetListBlocks()) << MD_BLOCK_SHIFT));

    return new;
}

//Calculates the address offset bytes away of base_address.
void *GetAddress(uint64_t offset, void *base_address){
    void *new = (void *) (offset + (char *) base_address);

    return new;
}

//Returns the index of the first least significat zero-bit. The operation has
//O(logn) complexity, where n the number of bits; in our case O(log64).
int BitmapFindZeroBit(uint64_t bitmap){
    uint64_t reversed = ~bitmap;
    int len = 64, start = 0, end = 63;

    uint64_t masks[] = {0xFFFFFFFF, 0xFFFF, 0xFF, 0xF, 0x3, 0x1};

    for(int i = 0; i < 6; i++){
        len >>= 1;

        if(reversed & masks[i]){
            reversed = reversed & masks[i];
            end -= len;

        }else{
            reversed = reversed >> len;
            start += len;
        }
    }

    return start;
}

//---------------------------------------------------------------
//Metadata File Operations

/*Initializes the metadata segment of the cib-file. The metadata segment will have 
the amount of list_blocks and node_blocks specified by the parameters.*/
void MDInit(uint32_t list_blocks, uint32_t node_blocks){
    CIBEntry root = CIBEntryCreate(NULL, ".");

    HeadSetMDSize((1 + node_blocks + list_blocks) * MD_BLOCK_SIZE);

    HeadSetNestLevel(0);
    HeadSetListBlocks(list_blocks);
    HeadSetListEntries(1);
    for(int i = 1; i < list_blocks; i++)
        CIBListBlockInit(i);
    
    FreeListInit(node_blocks);
    CIBListInit(root);

    free(root);
    return;
}

/*Inserts the first "component" of the given path that does not exist insinde the cib file.

If no such "component" exists then the last "component" of the path is updated.
We consider the path to be relative to the directory defined by the entry id "current_id".

For example if the path base_dir/dirA/dirB/dirC/ exists inside cib file and the entry id
of base_dir/dirA is 13 the running MDUpdatePath(entry, "dirB/dirC/file1.txt", 13, inserted)
will result in inserting file1.txt.

On the other hand if the path base_dir/dirA exists inside cib-file and its entry_id is 7, whilst
base_dir/dirA does not contain dirD, then running MDInsertPath(entry, "dirD/file2.txt", 7, inserted) will result
in inserting "dirD" and not "dirD/file2.txt" as you may expect.*/
EntryId MDUpdatePath(CIBEntry entry, char *path, EntryId start_id, bool *inserted){
    EntryId updated_id = CIBListUpdateEntry(entry, path, start_id, inserted);

    return updated_id;
}

/*Returns the entry_id of the entity defined by "path". We consider "path" to be relative
to the directory specified by "start_id".

If found, the flag "found" is set to true. Otherwise, the flag is set to false.*/
EntryId MDGetPath(char *path, EntryId start_id, bool *found){
    EntryId target_id = CIBListGetEntry(start_id, path, found);

    return target_id;
}

/*Prints the structure of the CIB file.*/
void MDPrintStructure(){
    CIBListPrintStructure(0, "/");

    return;
}

/*Prints information about every entity inside the CIB file.*/
void MDPrintEntriesMetadata(){
    CIBListPrintEntriesMetadata();

    return;
}

/*Returns a list with the entry_ids that the directory under start_id defined by "path" contains.
If the entity defined by path and start_id is not a directory, NULL is returned.*/
List MDGetDirEntries(EntryId dir_id){
    return CIBListGetDirEntries(dir_id);
}

/*Deletes the entry specified by entry_id. If it is a directory then its content
is also deleted.*/
void MDDeleteEntry(EntryId entry_id, EntryId parent_id){
    CIBListDeleteEntry(entry_id, parent_id);

    return;
}