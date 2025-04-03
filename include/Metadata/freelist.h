#include "metadata.h"

/*Initializes the free list.*/
void FreeListInit(uint32_t free_nodes);

/*Inserts the given cib_node block in the free-list struct.*/
void FreeListInsertNodeBlock(MDBlockId block);

/*Increases the amount of node blocks by the specified amount.*/
void FreeListIncreaseNodeBlocks(uint32_t nblocks);

/*Returns the id of a free block. The block can then be used as a cib-node block.

If there are no free-blocks then the file is truncated.*/
MDBlockId FreeListRequestNodeBlock();

/*Increases the size of the list by the specified amount of blocks.*/
void FreeListIncreaseListSize(uint32_t blocks);