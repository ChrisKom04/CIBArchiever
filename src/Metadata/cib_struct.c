#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <pwd.h>
#include <grp.h>

#include "metadata.h"
#include "header.h"
#include "freelist.h"
#include "data.h"

#include "syscalls.h"
#include "ADTVector.h"

#define LIST_BLOCK_EMPTY 0x80000000

typedef uint64_t EntryId;

#define max(a,b) ((a) > (b) ? (a) : (b))

/*In metadata partition we split the address space in blocks of size MD_BLOCK_SIZE.*/

/*Entries are stored inside the list. Each entry has a "pointer"
pointing to either a cib-node-block or to a data block.*/
typedef struct cib_entry{
    uint32_t uid;
    uint32_t gid;
    uint32_t mode;
    uint32_t created;
    uint32_t modified;
    uint32_t accessed;
    uint64_t pointer;

}* CIBEntry;

/*In the metadata partition we have an array, which we call CIBList, that stores
cib_entries. This array expands in continuous blocks of memory, which we call list_blocks. For each entity 
(directory/link/file) we hold a cib_entry which holds its metadata information as well as a "pointer"
to a data_block, if it's a link or file, or a cib_node, if it's a directory.

The position of a cib_entry in the list is defined by its EntryId. The EntryId is in essence the cell
in which the cib_entry is stored. Thus, in order to fine the list_block where a cib_entry is stored we
perform EntryId / LIST_ENTRIES_PER_BLOCK, where LIST_ENTRIES_PER_BLOCK is the number of cib_entries that
a list_block can hold.

In order to effectively track free spots for inserting cib_entries we save two bitmaps. The bitmap "bitmap"
referes to the cells of the list block and the bitmap "list_block_bitmap" refers to sets of list_blocks. You
can finde more details about these in README file.*/
typedef struct cib_list_block{
    uint32_t count;
    uint32_t bitmap;
    struct cib_entry entries[LIST_ENTRIES_PER_BLOCK];

    uint64_t list_block_bitmap;
    char padding[16];
}* CIBList;

/*CIBNodes are used by directory entries and hold pairs of <name, entry_id>. The cib_nodes may expand
to multiple blocks thus for every cib_node we save the block id of the previous and the next one.

The corresponding flags are set to 1/0 if there exists/not exists a next/previous cib_node block.*/
typedef struct cib_node{
    char name[3][256];
    uint64_t entry[3];

    uint32_t count;
    uint32_t self;
    uint32_t parent;    

    uint32_t next;
    uint32_t previous;
    uint8_t next_flag;
    uint8_t previous_flag;
    char padding[210];
}* CIBNode;

extern void *md;
extern void *data;

//---------------------------------------------------------------
//CIB-Node Functions

/*Initializes the given block as a cib-node block with the given entry ids
as parent and self.*/
void CIBNodeInit(MDBlockId block, EntryId parent, EntryId self){
    CIBNode node = GetNodeBlockAddress(block);

    memset(node, 0, MD_BLOCK_SIZE);
    node->parent = parent;
    node->self = self;

    return;
}

/*Sets the "previous" pointer of cib-node block to point to the given block.*/
void CIBNodeSetPrevious(MDBlockId node_block, MDBlockId previous){
    CIBNode node = GetNodeBlockAddress(node_block);

    node->previous = previous;
    node->previous_flag = true;

    return;
}

/*Sets the "next" pointer of cib-node block to point to the given block.*/
void CIBNodeSetNext(MDBlockId node_block, MDBlockId next){
    CIBNode node = GetNodeBlockAddress(node_block);

    node->next = next;
    node->next_flag = true;

    return;
}

/*Inserts <entry_id, entry_name> in the given cib-node block. Keep in mind that the
function does not examine whether an entry with the given name already exists in the
cib-node.

If the block is full then the function searches the list of cib_node blocks to find
a block with enough space. If no such block is found then one is created.*/
void CIBNodeInsertEntry(MDBlockId block, EntryId entry_id, char *entry_name){
    CIBNode node = GetNodeBlockAddress(block);

    //If node is full and the are no following nodes, we create one
    //and perform the insertion in the new block.
    if(node->count == 3 && node->next_flag == false){
        MDBlockId new_block = FreeListRequestNodeBlock();
        node = GetNodeBlockAddress(block);

        CIBNodeInit(new_block, node->parent, node->self);
        
        CIBNodeSetPrevious(new_block, block);
        CIBNodeSetNext(block, new_block);

        CIBNodeInsertEntry(node->next, entry_id, entry_name);

    }else if(node->next_flag == true){
        CIBNodeInsertEntry(node->next, entry_id, entry_name);

    }else{
        //Track down the free entry.
        for(int i = 0; i < 3; i++){
            //No directory can contain the entry id of the
            //root directory.
            if(node->entry[i] == 0){
                node->count++;
                node->entry[i] = entry_id;
                strcpy(node->name[i], entry_name);

                break;
            }
        }
        
    }

    return;
}

/*Searches the cib-node that starts from the given block to find an entry with
the given name. If found, *found is set to true and the entry id is returned.
Otherwise, *found is set to false.*/
EntryId CIBNodeGetEntry(MDBlockId block, char *entry_name, bool *found){
    CIBNode node = GetNodeBlockAddress(block);
    if(strcmp(entry_name, ".") == 0){
        *found = true;
        return node->self;

    }else if(strcmp(entry_name, "..") == 0){
        *found = true;
        return node->parent;

    }

    if(node->count > 0){
        for(int i = 0; i < 3; i++){
            if(strcmp(node->name[i], entry_name) == 0){
                
                *found = true;
                return node->entry[i];
            }
        }
    }

    if(node->next_flag == true)
        return CIBNodeGetEntry(node->next, entry_name, found);

    *found = false;
    return 0;
}

/*Deletes the given node block and inserts it in free list, iff
it's not the only block of the cib-node.

The pointers' of the previous and next blocks are updated.

If that block was the first one then the next block's content is copied to "block"
and afterwards the next one gets deleted.*/
void CIBNodeDeleteNodeBlock(MDBlockId block){
    CIBNode node = GetNodeBlockAddress(block);

    //If the block is not the first in the list, we can delete it and modify
    //the pointers of the previous and next blocks.
    if(node->previous_flag == true){
        CIBNode previous = GetNodeBlockAddress(node->previous);
        previous->next = node->next;
        previous->next_flag = node->next_flag;

    //If the block is the first in the list, we copy the content of the next block and
    //modify the content of the next block and the next block's next. Afterwards, we delete
    //the next block.
    }else if(node->next_flag == true){
        MDBlockId next_block = node->next;
        CIBNode next = GetNodeBlockAddress(next_block);

        next->previous_flag = false;
        memcpy(node, next, MD_BLOCK_SIZE);
        
        if(node->next_flag == true){
            CIBNode next_next = GetNodeBlockAddress(node->next);
            next_next->previous = block;

        }

        FreeListInsertNodeBlock(next_block);
    }

    return;
}

/*Deletes the entry with the given entry id from the cib node, given that "block" is the first
block of the cib-node.*/
void CIBNodeRemoveEntryId(MDBlockId block, EntryId entry_id){
    CIBNode node = GetNodeBlockAddress(block);

    for(int i = 0; i < 3; i++){
        if(node->entry[i] == entry_id){

            node->entry[i] = 0;
            strcpy(node->name[i], "");
            node->count--;

            if(node->count == 0 && (node->previous_flag == true || node->next_flag == true))
                CIBNodeDeleteNodeBlock(block);

            return;
        }
    }

    if(node->next_flag == true)
        CIBNodeRemoveEntryId(node->next, entry_id);

    return;
}

/*Inserts in the given list the pairs <entry_id, entry_name> of the cib_node. If the cib_node expands
on multiple blocks then this function is called on them too.*/
void CIBNodeGetDirEntries(MDBlockId block, List entries){
    CIBNode node = GetNodeBlockAddress(block);

    for(int i = 0; i < 3; i++){
        if(node->entry[i] != 0)
            ListInsertFirst(entries, INPairCreate(node->entry[i], node->name[i]));

    }

    if(node->next_flag == true)
        CIBNodeGetDirEntries(node->next, entries);

    return;
}

//---------------------------------------------------------------
//CIB-Entry Functions

/*Calculates and returns the address of the given entry_id.*/
CIBEntry GetEntryAddress(EntryId entry){
    return &((CIBList) GetListBlockAddress(entry / LIST_ENTRIES_PER_BLOCK))->entries[entry % LIST_ENTRIES_PER_BLOCK];
}

/*Given a pointer to a cib-entry struct and a path
the function copies some of the lstat's returned info
to the cib-entry. 

Entry's pointer is set to 0. It can be later changed with CIBEntrySetPointer().

If mem == NULL then a cib_entry struct is allocated in heap.*/
CIBEntry CIBEntryCreate(CIBEntry mem, char *path){
    struct stat info;
    if(lstat(path, &info) == -1)
        return NULL;

    if(mem == NULL)
        mem = malloc(sizeof(struct cib_entry));

    mem->uid = info.st_uid;
    mem->gid = info.st_gid;
    mem->mode = info.st_mode;
    mem->modified = (uint32_t) info.st_mtime;
    mem->accessed = (uint32_t) info.st_atime;
    mem->created = (uint32_t) info.st_ctime;
    mem->pointer = 0;

    return mem;
}

/*Return true or false depending on whether the given entry
represents a directory.*/
bool CIBEntryIsDir(CIBEntry entry){
    return (entry->mode & __S_IFMT) == __S_IFDIR; 
}

/*Return true or false depending on whether the given entry
represents a link.*/
bool CIBEntryIsLink(CIBEntry entry){
    return (entry->mode & __S_IFMT) == __S_IFLNK;
}

/*Return true or false depending on whether the given entry
represents a file.*/
bool CIBEntryIsFile(CIBEntry entry){
    return (entry->mode & __S_IFMT) == __S_IFREG;
}

/*Copies everything, includeing the pointer, from src entry
to dest.*/
void CIBEntryInit(EntryId dest, CIBEntry src){
    CIBEntry entry = GetEntryAddress(dest);
    memcpy(entry, src, sizeof(struct cib_entry));

    return;
}

/*Copies everything, apart from the pointer, from "new"
to the cib entry with the given entry_id.*/
void CIBEntryUpdate(EntryId entry_id, CIBEntry new){
    CIBEntry entry = GetEntryAddress(entry_id);

    entry->created = new->created;
    entry->modified = new->modified;
    entry->accessed = new->accessed;
    entry->gid = new->gid;
    entry->uid = new->uid;
    entry->mode = new->mode;

    return;
}

/*Sets the pointer of the entry with entry id "entry_id" to "pointer".*/
void CIBEntrySetPointer(EntryId entry_id, uint64_t pointer){
    CIBEntry entry = GetEntryAddress(entry_id);

    entry->pointer = pointer;
    return;
}

/*Obtains the pointer of the entry with entry id "entry_id".*/
uint64_t CIBEntryGetPointer(EntryId entry_id){
    CIBEntry entry = GetEntryAddress(entry_id);

    return entry->pointer;
}

//---------------------------------------------------------------
//CIB-List Functions

/*Initializes the given list block.*/
void CIBListBlockInit(ListBlock block){
    CIBList list = GetListBlockAddress(block);
    memset(GetListBlockAddress(block), 0, MD_BLOCK_SIZE);
    
    list->bitmap = LIST_BLOCK_EMPTY;
    return;
}

/*Jumps from set of blocks that contains at least one free entry to a smaller set of set that also contains a
free entry. When the nest_level reaches 0 then it tracks the list_block inside the set that has at least one 
free entry and obtains that entry.*/
EntryId CIBListGetFreeSpotRec(uint32_t list_blocks, ListBlock first_of_set, uint8_t nest_level, bool *full){
    if(nest_level == 0){     
        //Get the pointer to the "current" list block, which knows which list blocks in the set
        //have free space.
        CIBList list = GetListBlockAddress(first_of_set);
        //Find the list block in the current set that has free space to hold an entry.
        ListBlock insert_block = first_of_set + BitmapFindZeroBit(list->list_block_bitmap);

        //If all the blocks in the set are full but there is space for more blocks
        //then we create one.
        if(insert_block == list_blocks){
            FreeListIncreaseListSize(1);

            //Obtain the pointer again as it may change due to map/unmap.
            list = GetListBlockAddress(first_of_set);
        }

        //The list block with the free space.
        CIBList insert_list = GetListBlockAddress(insert_block);
        int index = BitmapFindZeroBit(insert_list->bitmap);

        //Update the stats.
        insert_list->count++;
        insert_list->bitmap |= 1 << index;

        //If the list block is now full we update the set's stats.
        if(insert_list->bitmap == (uint32_t) -1){
           
            list->list_block_bitmap |= 1ULL << (insert_block - first_of_set);
            *full = list->list_block_bitmap == (uint64_t) -1;

        }else{
            *full = false;

        }

        // printf("%lb\t ", list->list_block_bitmap);
        //Calculate the entry id and return.
        // printf("Nest Level: %u\t Insert Block: %d\t Index: %d\n", HeadGetNestLevel(), insert_block, index);
        return insert_block * LIST_ENTRIES_PER_BLOCK + index;
    }
    
    //Obtain pointer to list block that holds stats about the set of sets.
    CIBList list = GetListBlockAddress(first_of_set + nest_level);
    //Obtain the set that contains sets/blocks with free space.
    int index = BitmapFindZeroBit(list->list_block_bitmap);

    //Calculate first block of new set as well as the target block, which will
    //contain info about the subsets.
    ListBlock target_set = first_of_set + (index << (6 * nest_level));
    ListBlock target_block = target_set + nest_level - 1;

    //If the target set does not exist we create its first block. Only if
    //the block becomes full, new blocks will be created.
    if(target_set == list_blocks){
        FreeListIncreaseListSize(1);
        list_blocks++;
        //Recalculate the list block as the file may have been unmmaped/mmapped.
        list = GetListBlockAddress(first_of_set);
    }

    //To satisfy the condition above we may have to jump to a block whose nest level is less
    //than nest_level - 1. We calculate the difference and call the recursive function again.
    // fprintf(stderr, "Target: %u\t Index: %d\n", target_set, index);
    int diff = target_block >= list_blocks ? target_block - list_blocks + 1 : 0;
    EntryId id = CIBListGetFreeSpotRec(list_blocks, target_set, nest_level - 1 - diff, full);

    //Update current list block's stats. We may have to recalculate the address of the list
    //block as mmaps/remmaps may have been performed.
    if(*full == true){
        list = GetListBlockAddress(first_of_set + nest_level);
        list->list_block_bitmap |= 1ULL << index;

        *full = list->list_block_bitmap == (uint64_t) -1;
    }

    return id;
}

/*Finds a free entry id in the cib list and marks it as used. If none exists then a list block is created.

#Reads: O(log_{64}(n)), where n is the number of List Blocks.*/
EntryId CIBListGetFreeSpot(){
    int nest_level = HeadGetNestLevel();
    bool full;
    EntryId free_id = CIBListGetFreeSpotRec(HeadGetListBlocks(), 0, nest_level, &full);

    if(full == true){
        HeadSetNestLevel(++nest_level);
        CIBList list = GetListBlockAddress(nest_level);

        list->list_block_bitmap = 1;
    }

    return free_id;
}

/*When a block is set from full to not-full, this function is called to update the bitmaps of the
sets in which it belongs.*/
void CIBListUpdateGroupBitmap(ListBlock inserted_block, uint8_t nest_level, uint8_t max_nest){
    ListBlock first_of_set = (inserted_block & ~((64 << (6 * nest_level)) - 1));
    ListBlock pos_in_set = inserted_block - first_of_set;

    CIBList list = GetListBlockAddress(first_of_set + nest_level);

    uint32_t subset = pos_in_set >> (6 * nest_level);

    if(list->list_block_bitmap == (uint64_t) -1 && nest_level < max_nest)
        CIBListUpdateGroupBitmap(inserted_block, nest_level + 1, max_nest);

    list->list_block_bitmap &= ~(1 << subset);
    return;
}

/*Marks the spot in the cib list, specified by the given entry_id, as free.*/
void CIBListFreeEntry(EntryId entry_id){
    ListBlock inserted_block = entry_id / LIST_ENTRIES_PER_BLOCK;
    uint32_t index = entry_id % LIST_ENTRIES_PER_BLOCK;

    CIBList inserted_list = GetListBlockAddress(inserted_block);

    if(inserted_list->bitmap == (uint32_t) -1)
        CIBListUpdateGroupBitmap(inserted_block, 0, HeadGetNestLevel());
    

    inserted_list->bitmap &= ~(1 << index);
    inserted_list->count--;

    memset(GetEntryAddress(entry_id), 0, sizeof(struct cib_entry));
    return;
}

/*Returns the entry id of the entry defined by the path given, which is relative to the path
of the directory specified by the "current_id".

Found is set to true if the requested entry was found. Otherwise it is set to false.*/
EntryId CIBListGetEntry(EntryId current_id, char *path, bool *found){
    if(strcmp(path, "/") == 0 || strcmp(path, ".") == 0){
        *found = true;
        return current_id;
    }

    char copy[strlen(path) + 1]; strcpy(copy, path);
    CIBEntry current = GetEntryAddress(current_id);

    //Iterate throught the directories of the path.
    for(char *iter = strtok(copy, "/"); iter != NULL; iter = strtok(NULL, "/")){
        //If the iterator is not a directory and it is not the last "entity" in the path,
        //then the given path does not exit inside the cib-file.
        if(CIBEntryIsDir(current) == true){
            //Make sure that the entity iter-n is under directory iter-(n-1).
            current_id = CIBNodeGetEntry(current->pointer, iter, found);
            if(*found == false)
                return 0;

            current = GetEntryAddress(current_id);

        }else if(strtok(NULL, "/") != NULL){
            *found = false;

            return 1;
        }

    }
    
    *found = true;
    return current_id;
}

/*User needs to make sure that parent_id refers to a directory entry.
If entry_id refers to a directory, a cib_node will be also created.*/
void CIBListInsertEntryUnderDir(EntryId entry_id, EntryId parent_id, char *name){

    CIBEntry parent = GetEntryAddress(parent_id);
    CIBNodeInsertEntry(parent->pointer, entry_id, name);

    CIBEntry entry = GetEntryAddress(entry_id);
    if(CIBEntryIsDir(entry) == true){
        MDBlockId block = FreeListRequestNodeBlock();

        CIBNodeInit(block, parent_id, entry_id);
        CIBEntrySetPointer(entry_id, block);
    }

    return;
}

/*Inserts the first "component" of the given path that does not exist insinde the cib file.

If no such "component" exists then the last "component" of the path is updated.
We consider the path to be relative to the directory defined by the entry id "current_id".*/
EntryId CIBListUpdateEntry(CIBEntry entry, char *path, EntryId current_id, bool *inserted){
    char copy[strlen(path) + 1]; strcpy(copy, path);

    EntryId parent_id = CIBListGetEntry(current_id, dirname(copy), inserted);

    if(*inserted == false || CIBEntryIsDir(GetEntryAddress(parent_id)) == false){
        char buff[128 + strlen(path)];
        snprintf(buff, sizeof(buff), "./cib: Error: Path %s under entry with ID: %lu is not a directory or does not exist.\n", path, current_id);
        
        WriteBytes(buff, strlen(buff), 2);
        *inserted = false;
        return 0;
    }

    strcpy(copy, path);
    EntryId new_id = CIBListGetEntry(parent_id, basename(path), inserted);

    if(*inserted == false){
        new_id = CIBListGetFreeSpot();
        CIBEntryInit(new_id, entry);

        strcpy(copy, path);
        CIBListInsertEntryUnderDir(new_id, parent_id, basename(copy));

        HeadSetListEntries(HeadGetListEntries() + 1);
        *inserted = true;

        return new_id;

    }else if((GetEntryAddress(new_id)->mode & __S_IFMT) != (entry->mode & __S_IFMT)){
        *inserted = false;
        WriteBytes("Error: Cannot update a directory with a file or vice versa.\n", 60, 2);

        return 0;

    }else{
        CIBEntryUpdate(new_id, entry);
        *inserted = true;

    }

    return new_id;
}


void CIBListDeleteEntry(EntryId entry_id, EntryId parent_id);

/*Deletes the directory specified by the given entry id. The function also deletes
everything under that directory.*/
void CIBListDeleteDirEntry(EntryId entry_id){
    CIBEntry entry = GetEntryAddress(entry_id);
    MDBlockId block = entry->pointer;
    CIBNode node = GetNodeBlockAddress(block);
    bool next_flag;

    do{
        next_flag = node->next_flag;

        //Goes through the first cib_node and deletes every entry.
        for(int i = 0; i < 3; i++){

            if(node->entry[i] != 0){
                CIBListDeleteEntry(node->entry[i], entry_id);

            }
        }

    }while(next_flag == true);

    return;
}

/*Deletes the entry specified by entry_id. If it is a directory then its content
is also deleted.*/
void CIBListDeleteEntry(EntryId entry_id, EntryId parent_id){
    CIBEntry parent = GetEntryAddress(parent_id);
    
    CIBEntry entry = GetEntryAddress(entry_id);
    if(CIBEntryIsDir(entry) == true)
        CIBListDeleteDirEntry(entry_id);

    CIBNodeRemoveEntryId(parent->pointer, entry_id);

    CIBListFreeEntry(entry_id);
    HeadSetListEntries(HeadGetListEntries() - 1);   

    return;
}

/*Prints the struct of the directory with id "current_id" and name "name". This funtion
is also called on its subdirectories.*/
void CIBListPrintStructure(EntryId current_id, char *name){
    CIBEntry current = GetEntryAddress(current_id);
    MDBlockId block = current->pointer;
    CIBNode node;
    bool next_flag;

    char buff[4096];
    snprintf(buff, 4096, "%sDirectory: %lu. %s\n", current_id != 0 ? "\n" : "", current_id, name);
    WriteBytes(buff, strlen(buff), 1);

    strcpy(buff, "-------------------------------------------------------------------------\n");
    WriteBytes(buff, strlen(buff), 1);

    do{
        node = GetNodeBlockAddress(block);
        for(int i = 0; i < 3; i++){
            if(node->entry[i] != 0){
                snprintf(buff, 4096, "%6lu. %s\n", node->entry[i], node->name[i]);

                WriteBytes(buff, strlen(buff), 1);
            }
        }

        next_flag = node->next_flag;
        MDBlockId next = node->next;

        block = next;
    }while(next_flag == true);
    WriteBytes("-------------------------------------------------------------------------\n\n", 75, 1);

    block = current->pointer;
    do{
        node = GetNodeBlockAddress(block);
        for(int i = 0; i < 3; i++){
            if(node->entry[i] != 0 && CIBEntryIsDir(GetEntryAddress(node->entry[i])) == true)
                CIBListPrintStructure(node->entry[i], node->name[i]);

        }

        next_flag = node->next_flag;
        MDBlockId next = node->next;

        block = next;
    }while(next_flag == true);

    return;
}

/*Prints the Metadata for each entry stored in the metadata partition.*/
void CIBListPrintEntriesMetadata(){
    for(uint32_t i = 0; i < HeadGetListBlocks(); i++){
        CIBList list = GetListBlockAddress(i);

        for(uint64_t j = 0; j < LIST_ENTRIES_PER_BLOCK; j++){
            if(list->bitmap & (1 << j)){
                CIBEntry entry = GetEntryAddress(i * LIST_ENTRIES_PER_BLOCK + j);

                char mode[] = "----------";
                switch(entry->mode & __S_IFMT){
                    case __S_IFDIR: mode[0] = 'd'; break;
                    case __S_IFREG: mode[0] = '-'; break;
                    case __S_IFLNK: mode[0] = 'l'; break;
                }

                if(entry->mode & S_IRUSR) mode[1] = 'r';
                if(entry->mode & S_IWUSR) mode[2] = 'w';
                if(entry->mode & S_IXUSR) mode[3] = 'x';
                if(entry->mode & S_IRGRP) mode[4] = 'r';
                if(entry->mode & S_IWGRP) mode[5] = 'w';
                if(entry->mode & S_IXGRP) mode[6] = 'x';
                if(entry->mode & S_IROTH) mode[7] = 'r';
                if(entry->mode & S_IWOTH) mode[8] = 'w';
                if(entry->mode & S_IXOTH) mode[9] = 'x';

                struct passwd *pw = getpwuid(entry->uid);
                struct group *gt = getgrgid(entry->gid);

                char *user_name = pw != NULL ?  pw->pw_name : "uknown";
                char *group_name = gt != NULL ? gt->gr_name : "uknown";

                time_t t = (time_t) entry->created;
                struct tm *tm_info = localtime(&t);
                char created[64];
                strftime(created, sizeof(created), "%Y-%m-%d %H:%M:%S", tm_info);

                t = (time_t) entry->modified;
                tm_info = localtime(&t);
                char modified[64];
                strftime(modified, sizeof(modified), "%Y-%m-%d %H:%M:%S", tm_info);

                t = (time_t) entry->accessed;
                tm_info = localtime(&t);
                char accessed[64];
                strftime(accessed, sizeof(accessed), "%Y-%m-%d %H:%M:%S", tm_info);
                
                char buff[1024 + strlen(user_name) + strlen(group_name)];
                snprintf(buff, sizeof(buff), "%lu. %s\t%s\t%s\tCreated: %s\tModified: %s\tAccessed: %s\n", i * LIST_ENTRIES_PER_BLOCK + j, mode, user_name, group_name, created, modified, accessed);
                WriteBytes(buff, strlen(buff), 1);
            }
        }
    }
}

/*Returns a list containing the pairs <entry_id, name> that the directory specified by the
entry_id contains. If the given entry_id is not a directory, NULL is returned.*/
List CIBListGetDirEntries(EntryId dir_id){
    CIBEntry dir = GetEntryAddress(dir_id);

    if(CIBEntryIsDir(dir) == false)
        return NULL;

    List entries = ListCreate((DestroyFunc) INPairDestroy);
    CIBNodeGetDirEntries(dir->pointer, entries);

    return entries;
}

/*Initializes the CIBList.*/
void CIBListInit(CIBEntry root){
    CIBListBlockInit(0);

    CIBList list = GetListBlockAddress(0);
    list->bitmap |= 1;
    list->count = 1;
    CIBEntryInit(0, root);

    MDBlockId block = FreeListRequestNodeBlock();
    CIBNodeInit(block, 0, 0);
    CIBEntrySetPointer(0, block);

    return;
}