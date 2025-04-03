#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

#include "metadata.h"
#include "header.h"
#include "cib_struct.h"
#include "file_management.h"


#define FREE_LIST_MAX_ENTRIES 253
#define max(a,b) ((a) > (b) ? (a) : (b))

//-------------------------------------------------------------
/*Free-List
Keeps Track of free blocks inside the metadata "partition".*/

typedef struct free_node{
    MDBlockId next;         //Id of the "next" free block.
    uint8_t next_flag;      //1 iff there is a "next" free block in the list.

    char padding[1019];
}* FreeNode;

/*Holds a circular buffer containing ids of free blocks. If this buffer goes full then the extra blocks
form a list whose head is stored in the field "header".*/
typedef struct free_list{
    uint32_t total_free;                               //Number of free node blocks in metadata segment.

    MDBlockId header;                                  //ID of the first block of the list that contains free blocks.
                              
    MDBlockId array[FREE_LIST_MAX_ENTRIES];            //Array containing IDs of free blocks. If there are > 253 free blocks then the remaining are added into the list.
    uint8_t arr_start;                               
    uint8_t arr_count;
    char padding[2];
}* FreeList;

extern void *md;

//-------------------------------------------------------------
//Free-Node Functions

/*Initializes a free-block.*/
void FreeNodeInit(MDBlockId block){
    FreeNode node = GetNodeBlockAddress(block);
    memset(node, 0, MD_BLOCK_SIZE);

    return;
}

/*Sets the next_flag flag of a free-block to false.*/
void FreeNodeRemoveNext(MDBlockId block){
    FreeNode node = GetNodeBlockAddress(block);

    node->next_flag = 0;
    return;
}

/*Sets the next_flag flag of a free-block to true.*/
void FreeNodeSetNext(MDBlockId block, MDBlockId next){
    FreeNode node = GetNodeBlockAddress(block);
    
    node->next_flag = 1;
    node->next = next;

    return;
}

/*Returns the id of the next free block in the list.*/
MDBlockId FreeNodeGetNext(MDBlockId block){
    FreeNode node = GetNodeBlockAddress(block);

    return node->next;
}

//-------------------------------------------------------------
//Free-List Functions

/*Returns the id of a free block. The block can then be used as a cib-node block.

If there are no free-blocks then the file is truncated.*/
MDBlockId FreeListRequestNodeBlock(){
    FreeList list = GetFreeListBlockAddress();

    if(list->arr_count != 0){
        HeadSetMDFreeNodeBlocks(--list->total_free);
        
        MDBlockId block = list->array[list->arr_start];
        list->arr_start = (list->arr_start + 1) % FREE_LIST_MAX_ENTRIES;
        list->arr_count--;
        return block;
    
    }else if(list->total_free){
        HeadSetMDFreeNodeBlocks(--list->total_free);

        MDBlockId block = list->header;
        list->header = FreeNodeGetNext(block);

        return block;

    }else{
        TruncMapAndUpdate(HeadGetHeaderSize(), HeadGetMDSize() + MD_BLOCK_SIZE, HeadGetDataSize(), true);

        MDBlockId new_block = (HeadGetMDSize() >> MD_BLOCK_SHIFT) - 1 - HeadGetListBlocks() - 1;

        return new_block;
    }
}

/*Increases the size of the list by the specified amount of blocks.*/
void FreeListIncreaseListSize(uint32_t blocks){
    TruncMapAndUpdate(HeadGetHeaderSize(), HeadGetMDSize() + blocks * MD_BLOCK_SIZE, HeadGetDataSize(), true);

    uint32_t list_blocks = HeadGetListBlocks();
    uint64_t md_size = HeadGetMDSize();
    memmove(GetListBlockAddress(list_blocks + blocks), GetListBlockAddress(list_blocks), md_size - MD_BLOCK_SIZE * (1 + list_blocks + blocks));

    HeadSetListBlocks(list_blocks + blocks);

    for(uint32_t i = 0; i < blocks; i++)
        CIBListBlockInit(list_blocks + i);
    
    return;
}

/*Inserts the given cib_node block in the free-list struct.*/
void FreeListInsertNodeBlock(MDBlockId block){
    FreeList list = GetFreeListBlockAddress();

    if(list->arr_count < FREE_LIST_MAX_ENTRIES){
        int index = (list->arr_count + list->arr_start) % FREE_LIST_MAX_ENTRIES;
        list->array[index] = block;

        list->arr_count++;

    }else{
        FreeNodeInit(block);
        if(list->total_free > FREE_LIST_MAX_ENTRIES)
            FreeNodeSetNext(block, list->header);
        
        list->header = block;
    }

    HeadSetMDFreeNodeBlocks(++list->total_free);
    return;
}

/*Increases the amount of node blocks by the specified amount.*/
void FreeListIncreaseNodeBlocks(uint32_t nblocks){
    TruncMapAndUpdate(HeadGetHeaderSize(), HeadGetMDSize() + nblocks * MD_BLOCK_SIZE, HeadGetDataSize(), true);

    for(uint32_t i = 1; i <= nblocks; i++)
        FreeListInsertNodeBlock((HeadGetMDSize() >> MD_BLOCK_SHIFT) - HeadGetListBlocks() - 1 - i);

    return;
}

/*Initializes the free list.*/
void FreeListInit(uint32_t free_nodes){
    FreeList list = GetFreeListBlockAddress();
    memset(list, 0, MD_BLOCK_SIZE);

    HeadSetMDFreeNodeBlocks(0);
    for(uint32_t i = 0; i < free_nodes; i++)
        FreeListInsertNodeBlock(i);

    return;
}