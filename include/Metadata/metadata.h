#include <stdint.h>
#include <stdbool.h>

#include "ADTList.h"

#define LIST_ENTRIES_PER_BLOCK 31
#define FREE_LIST_MAX_ENTRIES 253

#define MD_BLOCK_SIZE 1024
#define MD_BLOCK_SHIFT 10

#define MD_MINIMUM_BLOCKS 2
#define FREE_LIST_BLOCK 0
#define CIB_LIST_BLOCK 1

typedef uint32_t ListBlock;
typedef uint32_t MDBlockId;
typedef uint64_t EntryId;

/*Each entity inserted in the cib file is assigned an entry_id as well as a "spot"
in the CIB-List. This spot is considered a cib-entry.*/
typedef struct cib_entry *CIBEntry;

/*Given a pointer to a cib-entry struct and a path
the function copies some of the lstat's returned info
to the cib-entry. 

Entry's pointer is set to 0. It can be later changed with CIBEntrySetPointer().

If mem == NULL then a cib_entry struct is allocated in heap.*/
CIBEntry CIBEntryCreate(CIBEntry mem, char *path);

/*Given an entry id, the function returns a pointer to the cib_entry which is stored inside
the CIB file.

Be careful as the pointer may not be pointing to a mapped area after a insertion.*/
CIBEntry GetEntryAddress(EntryId id);

/*Sets the pointer of the entry with the given id to "pointer".*/
void CIBEntrySetPointer(EntryId entry_id, uint64_t pointer);

/*Returns the pointer of the entry with the given id.*/
uint64_t CIBEntryGetPointer(EntryId entry_id);

/*Return true or false whether or not the given entry is a directory.*/
bool CIBEntryIsDir(CIBEntry entry);

/*Return true or false whether or not the given entry is a link.*/
bool CIBEntryIsLink(CIBEntry entry);

/*Return true or false whether or not the given entry is a directory.*/
bool CIBEntryIsFile(CIBEntry entry);

//All the above are contained in cib_struct.c.

//Returns a pointer to the requested list-block.
void *GetListBlockAddress(uint64_t block_num);

//Returns a pointer to the requested node-block.
void *GetNodeBlockAddress(uint64_t block_num);

//Returns a pointer to the free-list block.
void *GetFreeListBlockAddress();

//Calculates the address offset bytes away of base_address.
void *GetAddress(uint64_t offset, void *base_address);

//Returns the index of the first least significat zero-bit. The operation has
//O(logn) complexity, where n the number of bits; in our case O(log64).
int BitmapFindZeroBit(uint64_t bitmap);

/*Initializes the metadata segment of the cib-file. The metadata segment will have 
the amount of list_blocks and node_blocks specified by the parameters.*/
void MDInit(uint32_t list_blocks, uint32_t node_blocks);

/*Inserts the first "component" of the given path that does not exist insinde the cib file.

If no such "component" exists then the last "component" of the path is updated.
We consider the path to be relative to the directory defined by the entry id "current_id".

For example if the path base_dir/dirA/dirB/dirC/ exists inside cib file and the entry id
of base_dir/dirA is 13 the running MDUpdatePath(entry, "dirB/dirC/file1.txt", 13, inserted)
will result in inserting file1.txt.

On the other hand if the path base_dir/dirA exists inside cib-file and its entry_id is 7, whilst
base_dir/dirA does not contain dirD, then running MDInsertPath(entry, "dirD/file2.txt", 7, inserted) will result
in inserting "dirD" and not "dirD/file2.txt" as you may expect.*/
EntryId MDUpdatePath(CIBEntry entry, char *path, EntryId start_id, bool *inserted);

/*Returns the entry_id of the entity defined by "path". We consider "path" to be relative
to the directory specified by "start_id".

If found, the flag "found" is set to true. Otherwise, the flag is set to false.*/
EntryId MDGetPath(char *path, EntryId start_id, bool *found);

/*Prints the structure of the CIB file.*/
void MDPrintStructure();

/*Prints information about every entity inside the CIB file.*/
void MDPrintEntriesMetadata();

/*Returns a list with the entry_ids that the directory under start_id defined by "path" contains.
If the entity defined by path and start_id is not a directory, NULL is returned.*/
List MDGetDirEntries(EntryId dir_id);

/*Deletes the entry specified by entry_id. If it is a directory then its content
is also deleted.*/
void MDDeleteEntry(EntryId entry_id, EntryId parent_id);

//INPair

/*A struct that holds Id-Name.*/
typedef struct id_name_pair *INPair;

/*Creates an INPair struct in heap and returns a pointer to it.*/
INPair INPairCreate(EntryId entry_id, char *name);

/*Returns the Id of the given pair.*/
EntryId INPairGetId(INPair pair);

/*Returns the name of the given pair.*/
char *INPairGetName(INPair pair);

/*Destroys the given pair.*/
void INPairDestroy(INPair pair);