#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>

#include "header.h"
#include "file_management.h"
#include "data.h"
#include "syscalls.h"

#define MD_FREE_LIST_ENTRIES 63
#define MD_FREE_LIST_BLOCK 0

extern void *md;
extern void *data;
extern void *header;

/*In this partition we split the address space in blocks of DATA_BLOCK_SIZE bytes.

Continuous blocks that are either free or used to store the data of a file form chunks. In every chunk,
its first byte (the first byte of its first block) is 1 if the chunk is used to store data or 0 if it is empty.
Furthermore, the last 8 bytes of each chunk (the last 8 bytes of the chunk's last block) contain the number of
blocks that the chunk holds.

This provides an easy way to merge chunks. When a chunk that contains data is freed then we can easily check whether
the previous or the next one are used. If not then we merge them creating a bigger chunk.*/

/*This struct represents the first data_block of a chunk that contain the data of a file/link.

The variable used is set to 1.*/
typedef struct file{
    uint8_t used;
    uint8_t zipped;         //1 iff the content is zipped. Unzip will be needed when extracting.

    char padding[6];

    uint64_t blocks;        //The number of blocks thata this chunk of blocks holds.
    uint64_t size;          //The size of data in bytes.
    char data[1000];        //Data starts from here.
}* File;

/*This struct represents the first data_block of a chunk that is free.

The variable used is set to 0.*/
typedef struct data_free_chunk{
    uint8_t used;
    uint8_t next_flag;          //True iff there is a "next" free chunk.
    uint8_t previous_flag;      //True iff there is a "previous" chunk.
    char padding1[5];

    uint64_t block_count;       //Number of blocks that this chunk contains.
    uint64_t previous_block;    //If "previous_flag" is true then this variable holds the id of the previous chunk's first block.
    uint64_t next_block;        //If "next_flag" is true then this variable holds the id of the next chunk's first block.

    char padding2[990];
}* DFreeChunk;

/*We need a way to identify the unused chunks and be able to provide them for storing data.
We use the first block of the data partition for that reason. 

It contains and array, in the form of a circulat buffer, of data_block ids, which refer to the first block of the chunk, 
and their respective size in blocks. This array can hold MD_FREE_LIST_ENTRIES block ids. If the number of free chunks 
exceeds this constant then the rest of the free chunks will form a linked list.

The chunks in the array, which "continues" as a linked list, remain sorted based on the chunk size.*/
typedef struct data_free_list{
    DataBlockId list_head;                          //Block ID of the chunk's first block that is the head of the linked list.

    uint64_t chunks[MD_FREE_LIST_ENTRIES];          //Holds the free chunks of data partition.
    uint64_t blocks_count[MD_FREE_LIST_ENTRIES];    //Holds the size of the free chunks.

    uint8_t arr_start;                              //The "start" of the array. In this index we hold the biggest chunk.
    uint8_t arr_count;                              //Number of entries in the array.
    uint8_t list_flag;                              //1 iff the array is full and we have a linked list of free chunks.
    
    char padding[5];
}* DFreeList;

//--------------------------------------------------------
//Address Caclulating Functions

/*Return the addresss of the given block.*/
void *DGetDBlockAddress(DataBlockId block){
    return (void *) ((char *) data + (block << DATA_BLOCK_SHIFT));
}

/*Returns the address of the data_free_list.*/
DFreeList DGetFreeListAddress(){
    return (void *) ((char *) data + (MD_FREE_LIST_BLOCK << DATA_BLOCK_SHIFT));
}

//--------------------------------------------------------
//Data-Free-Chunk Functions

/*Initializes a free chunk. Used is set to 0 and block count is written in the first block's field
as well as in the chunk's last 8 bytes.*/
void DFreeChunkInit(DataBlockId block, uint64_t block_count){
    DFreeChunk chunk = DGetDBlockAddress(block);
    memset(chunk, 0, DATA_BLOCK_SIZE);

    chunk->block_count = block_count;
    chunk->used = 0;
    *((uint64_t *) ((char *) DGetDBlockAddress(block + block_count) - sizeof(uint64_t))) = block_count;
    return;
}

/*Returns the first block id of the following chunk in the free list. Sets flag to true if a following chunk exists.
Otherwise flag is set to false.*/
DataBlockId DFreeChunkGetNext(DataBlockId block, bool *flag){
    DFreeChunk chunk = DGetDBlockAddress(block);
    *flag = chunk->next_flag == 1;

    return chunk->next_block;
}

/*Returns the first block id of the previous chunk in the free list. Sets flag to true if a previous chunk exists.
Otherwise flag is set to false.*/
DataBlockId DFreeChunkGetPrevious(DataBlockId block, bool *flag){
    DFreeChunk chunk = DGetDBlockAddress(block);
    *flag = chunk->previous_flag == 1;

    return chunk->previous_block;
}

/*Sets the block's next block id to be "next".*/
void DFreeChunkSetNext(DataBlockId block, DataBlockId next){
    DFreeChunk chunk = DGetDBlockAddress(block);

    chunk->next_flag = 1;
    chunk->next_block = next;
    return;
}

/*Sets the block's previous block id to be "previous".*/
void DFreeChunkSetPrevious(DataBlockId block, DataBlockId previous){
    DFreeChunk chunk = DGetDBlockAddress(block);

    chunk->previous_flag = 1;
    chunk->previous_block = previous;
    return;
}

/*Sets block's next_flag to be false.*/
void DFreeChunkRemoveNext(DataBlockId block){
    DFreeChunk chunk = DGetDBlockAddress(block);

    chunk->next_flag = 0;
    return;
}

/*Sets block's previous_flag to be false.*/
void DFreeChunkRemovePrevious(DataBlockId block){
    DFreeChunk chunk = DGetDBlockAddress(block);

    chunk->previous_flag = 0;
    return;
}


//--------------------------------------------------------
//Data-Free-List Functions

/*Initializes the block that holds the data_free_list.*/
void DFreeListInit(){
    DFreeList list = DGetFreeListAddress();
    memset(list, 0, DATA_BLOCK_SIZE);

    return;
}

/*Inserts the chunk with first block "start" and size "blocks_count" in the free list struct.*/
void DFreeListInsertChunk(DataBlockId start, uint64_t blocks_count){
    DFreeList list = DGetFreeListAddress();
    DFreeChunkInit(start, blocks_count);

    //Scan the array to find the appropriate index where the chunk should be inserted.
    int index = -1;
    for(int i = 0; i < list->arr_count; i++){
        int iter = (list->arr_start + i) % MD_FREE_LIST_ENTRIES;

        if(blocks_count > list->blocks_count[iter]){
            index = iter;
            break;
        }
    }

    //If the chunk is not full and index is -1 we insert the chunk at the last position of the array.
    if(index == -1 && list->arr_count != MD_FREE_LIST_ENTRIES){
        index = (list->arr_count + list->arr_start) % MD_FREE_LIST_ENTRIES;

        list->chunks[index] = start;
        list->blocks_count[index] = blocks_count;
        list->arr_count++;

    //If the chunk is not full and index != -1 we shift the the chunks after index + 1 and we insert the chunk in the 
    //index position
    }else if(index != -1 && list->arr_count != MD_FREE_LIST_ENTRIES){
        int last_index = (list->arr_start + list->arr_count - 1 + MD_FREE_LIST_ENTRIES) % MD_FREE_LIST_ENTRIES;

        for(int i = (last_index + 1) % MD_FREE_LIST_ENTRIES; i != index;){
            int previous = (i - 1 + MD_FREE_LIST_ENTRIES) % MD_FREE_LIST_ENTRIES;

            list->chunks[i] = list->chunks[previous];
            list->blocks_count[i] = list->blocks_count[previous];

            i = previous;
        }

        list->chunks[index] = start;
        list->blocks_count[index] = blocks_count;
        list->arr_count++;

    //If index is -1 and list is full we have to insert the chunk in the linked list.
    }else if(index == -1 && list->arr_count == MD_FREE_LIST_ENTRIES){

        //If there is no list yet we create it.
        if(list->list_flag == 0){
            list->list_flag = 1;
            list->list_head = start;

        //Otherwise, we scan the linked list and insert it in the right spot.
        }else{    
            bool flag = true; DataBlockId last;
            
            for(DataBlockId iter = list->list_head; flag == true; iter = DFreeChunkGetNext(iter, &flag)){
                DFreeChunk current = DGetDBlockAddress(iter);

                if(blocks_count >= current->block_count){
                    // start->iter
                    DFreeChunkSetNext(start, iter);
                    bool exists; DataBlockId previous = DFreeChunkGetPrevious(iter, &exists);
                    /*        ->
                        start    iter
                              <-
                    */
                    DFreeChunkSetPrevious(iter, start);

                    if(exists == true){
                        /*         ->       ->
                          previous    start    iter
                                   <-       <-
                        */
                        DFreeChunkSetPrevious(start, previous);
                        DFreeChunkSetNext(previous, start);

                    //If no previous exists then iter was the head of the list.
                    }else{
                        list->list_head = iter;
                    }

                    return;
                }

                //We need to save the last in case the given chunk is smallest free chunk.
                last = iter;
            }

            //We set the given chunk to be the last chunk.
            DFreeChunkSetNext(last, start);
            DFreeChunkSetPrevious(start, last);
        }

    //If we found a spot in the array for the given chunk buut the list is full then we need to move
    //the smallest chunk of the array in the list and then shift every chunk in the array from index "index + 1".
    //Afterwards, we can insert the given chunk.
    }else if(index != -1 && list->arr_count == MD_FREE_LIST_ENTRIES){
        int last_index = (list->arr_start + list->arr_count - 1 + MD_FREE_LIST_ENTRIES) % MD_FREE_LIST_ENTRIES;

        if(list->list_flag == 1){
            DFreeChunkSetNext(list->chunks[last_index], list->list_head);
            DFreeChunkSetPrevious(list->list_head, list->chunks[last_index]);

        }

        list->list_head = list->chunks[last_index];
        list->list_flag = 1;
        
        for(int i = last_index; i != index;){
            int previous = (i - 1) % MD_FREE_LIST_ENTRIES;

            list->chunks[i] = list->chunks[previous];
            list->blocks_count[i] = list->blocks_count[previous];

            i = previous;
        }

        list->chunks[index] = start;
        list->blocks_count[index] = blocks_count;
    }

    return;
}

/*Removes the free chunk with first block id "start" from the free list struct.

Make sure that the given chunk is a free chunk.*/
void DFreeListRemoveChunk(DataBlockId start){
    DFreeList list = DGetFreeListAddress();

    if(list->arr_count <= 1){
        list->arr_count = 0;

        return;
    }

    //Scan the array.
    int index = -1;
    for(int i = 0; i < list->arr_count; i++){
        int iter = (list->arr_start + i) % MD_FREE_LIST_ENTRIES;

        if(list->chunks[iter] == start){
            index = iter;
            break;
        }
    }

    //If found then we need to shift the chunks after it in order to fill the gap.
    if(index != -1){
        int last_index = (list->arr_count + list->arr_start - 1 + MD_FREE_LIST_ENTRIES) % MD_FREE_LIST_ENTRIES;

        //Perform the shifting.
        for(int i = index; i != last_index;){
            int next = (i + 1) % MD_FREE_LIST_ENTRIES;

            list->chunks[i] = list->chunks[next];
            list->blocks_count[i] = list->blocks_count[next];

            i = next;
        }

        if(list->list_flag == 0){
            list->arr_count--;
        
        //If there exists a linked list of free chunks then we need to move the head of the list in the last position
        //of the array.
        }else{
            DFreeChunk chunk = DGetDBlockAddress(list->list_head);

            list->chunks[last_index] = list->list_head;
            list->blocks_count[last_index] = chunk->block_count;

            bool flag; DataBlockId next = DFreeChunkGetNext(list->list_head, &flag);
            
            if(flag == true){
                list->list_head = next;
                DFreeChunkRemovePrevious(next);

            }else{
                list->list_flag = 0;
            
            }
        }

    //If not found in the array then the free chunk is part of the free list.
    }else if(list->list_flag == 1){
        bool prev_flag; bool next_flag;

        DataBlockId prev_id = DFreeChunkGetPrevious(start, &prev_flag);
        DataBlockId next_id = DFreeChunkGetNext(start, &next_flag);

        if(prev_id == true && next_id == true){
            DFreeChunkSetNext(prev_id, next_id);
            DFreeChunkSetPrevious(next_id, prev_id);

        }else if(prev_id == true){
            DFreeChunkRemoveNext(prev_id);

        }else if(next_id == true){
            DFreeChunkRemovePrevious(next_id);
            list->list_head = next_id;

        }else{
            list->list_flag = 0;

        }
    
    }else if(list->list_flag == 0)
        printf("We have a problem");

    return;
}

/*Returns the first block's id of a chunk of size "block_count".

The function selects a chunk and "cuts" from it a chunk of size "block_count".
The poly with which we select this chunk is "best-fit" from the chunks stored in the array.*/
DataBlockId DFreeListRequestChunk(uint64_t block_count){
    DFreeList list = DGetFreeListAddress();

    if(list->arr_count > 0 && list->blocks_count[list->arr_start] >= block_count){
        int previous = 0, current = 1;

        for(; current < list->arr_count; current++, previous++){
            int index = (list->arr_start + current) % MD_FREE_LIST_ENTRIES;

            if(list->blocks_count[index] < block_count)
                break;
        }
        
        DataBlockId target = list->chunks[(list->arr_start + previous) % MD_FREE_LIST_ENTRIES];
        uint64_t target_total_blocks = list->blocks_count[(list->arr_start + previous) % MD_FREE_LIST_ENTRIES];

        DFreeListRemoveChunk(target);

        if(target_total_blocks != block_count)
            DFreeListInsertChunk(target + block_count, target_total_blocks - block_count);

        return target;
        
    }else {
        printf("Needed Blocks\n");
        DataBlockId new_chunk = (HeadGetDataSize() >> DATA_BLOCK_SHIFT);

        uint64_t extra_space = (block_count << DATA_BLOCK_SHIFT);
        TruncMapAndUpdate(HeadGetHeaderSize(), HeadGetMDSize(), HeadGetDataSize() + extra_space, true);

        memmove(md, (char *)md - extra_space, HeadGetMDSize());
        return new_chunk;
    }
    
}

/*Scans the struct that holds the free chunks to find the chunk whose position is at the end of the
data "partition". If found then we shrink the data partition and shift the metadata partition, thus 
reducing the file's size.*/
void DFreeListRemoveLastChunk(){
    DFreeList list = DGetFreeListAddress();
    bool found = false; DataBlockId last;
    
    for(int i = 0; i < list->arr_count  && found == false; i++){
        int index = (i + list->arr_start) % MD_FREE_LIST_ENTRIES;

        if(list->chunks[index] + list->blocks_count[index] == (HeadGetDataSize() >> DATA_BLOCK_SHIFT)){
            last = list->chunks[index];
            DFreeListRemoveChunk(list->chunks[index]);

            found = true;
        }

    }

    if(list->list_flag == 1 && found == false){
        DataBlockId current = list->list_head; bool next_flag;

        do{
            DFreeChunk chunk = DGetDBlockAddress(current);

            if(chunk->block_count + current == (HeadGetDataSize() >> DATA_BLOCK_SHIFT)){
                DFreeListRemoveChunk(current);
                last = current;

                found = true;
            }

            current = chunk->next_block;
            next_flag = chunk->next_flag;

        }while(next_flag == true && found == false);
    }

    if(found == true){
        memmove(DGetDBlockAddress(last), md, HeadGetMDSize());

        TruncMapAndUpdate(HeadGetHeaderSize(), HeadGetMDSize(), last << DATA_BLOCK_SHIFT, true);
    }

    return;
}

//--------------------------------------------------------
//Data Functions

/*Calculates the amount of blocks needed to store the given bytes of data.*/
uint64_t DataCaclulateNeededBlocks(uint64_t size){
    uint64_t required_blocks = ((size + FILE_EXTRA_DATA) >> DATA_BLOCK_SHIFT) + (((size + FILE_EXTRA_DATA) & (DATA_BLOCK_SIZE - 1)) > 0);

    return required_blocks;
}


/*Copies size bytes from the given address in memmory. If zipped is true then data are marked as zipped.*/
DataBlockId DataInsertBytes(void *mem, uint64_t size, bool zipped){
    uint64_t required_blocks = ((size + FILE_EXTRA_DATA) >> DATA_BLOCK_SHIFT) + 1;
    DataBlockId block = DFreeListRequestChunk(required_blocks);
    File dest = DGetDBlockAddress(block);


    dest->blocks = required_blocks;
    dest->size = size;
    dest->used = 1;
    dest->zipped = zipped == true;
    
    memcpy(dest->data, mem, size);
    *(uint64_t *) ((char *) DGetDBlockAddress(block + required_blocks) - sizeof(uint64_t)) = required_blocks;

    return block;
}

/*Inserts the data of the file defined by the given path inside the data "partition".
If zipped is true then data are marked as zipped.*/
DataBlockId DataInsertFile(char *path, bool zipped){
    int fd;
    if(OpenFile(path, &fd, O_RDONLY, 0644) == -1){
        printf("Shit\n");}

    uint64_t size = lseek(fd, 0, SEEK_END);

    void *mem = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    
    DataBlockId block = DataInsertBytes(mem, size, zipped);

    munmap(mem, size);
    close(fd);

    return block;
}

/*Inserts the stored path of the symlink, which is defined by path, in data partition.*/
DataBlockId DataInsertLink(char *path){
    int max_size = 4096;
    char *buff = calloc(1, 4096);

    while(readlink(path, buff, max_size - 1) == (max_size - 1)){
        free(buff);
        
        buff = calloc(1, 2 * max_size);
        max_size *= 2;
    }

    DataBlockId block = DataInsertBytes(buff, strlen(buff), 0);
    
    free(buff);
    return block;
}

/*Deletes the file which is stored in data partition starting from the given block.*/
void DataDeleteFile(DataBlockId block){
    
    File target = DGetDBlockAddress(block);
    uint64_t new_chunk_size = target->blocks;

    if(((block + target->blocks) << DATA_BLOCK_SHIFT) < HeadGetDataSize()){
        DataBlockId next_id = block + target->blocks;
        uint8_t next_used = *((uint8_t *) DGetDBlockAddress(next_id));

        if(next_used == 0){
            DFreeChunk next = DGetDBlockAddress(next_id);

            new_chunk_size += next->block_count;
            DFreeListRemoveChunk(next_id);
        }
    }

    if(block > 1){
        DataBlockId previous_id = block - *(uint64_t *)((char *) target - sizeof(uint64_t));
        uint8_t previous_used = *((uint8_t *) DGetDBlockAddress(previous_id));

        if(previous_used == 0){
            DFreeChunk previous = DGetDBlockAddress(previous_id);

            new_chunk_size += previous->block_count;
            DFreeListRemoveChunk(previous_id);
            
            block = previous_id;
        }
    }

    DFreeChunkInit(block, new_chunk_size);
    DFreeListInsertChunk(block, new_chunk_size);
    return;
}

/*Extracts the file that is stored in the data chunk whose first block is "block" inside
the file defined by path. The file is opened/created with the given permissions.

If the file was zipped then fork and exec are called to unzip the file using "gunzip".
This function will not use wait() to collect the zombie process created.

Returns true if the file was zipped or false if it wasn't.*/
bool DataExtractFile(DataBlockId block, char *path, int perm){
    File src = DGetDBlockAddress(block);

    char *real_path;
    if(src->zipped == 1 && src->size != 0){
        real_path = malloc(sizeof(char) * (strlen(path) + 4));
        snprintf(real_path, strlen(path) + 4, "%s.gz", path);

    }else{
        real_path = strdup(path);

    }

    int file_desc;
    if(OpenFile(real_path, &file_desc, O_RDWR | O_CREAT | O_TRUNC, 0644) == -1)
        return false;
    
    if(src->size == 0){
        close(file_desc); free(real_path);
        return false;
    }

    ftruncate(file_desc, src->size);
    void *target = mmap(NULL, src->size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED, file_desc, 0);
    
    if(target == MAP_FAILED){
        switch(errno){
            case EACCES: printf("EACCES\n"); break; 
            case EAGAIN: printf("EAGAIN\n"); break;
            case EBADF: printf("EBADF\n"); break;
            case EEXIST: printf("EEXIST\n"); break;
            case EINVAL: printf("EINVAL\n"); break;
        }
    }
        

    memcpy(target, src->data, src->size);

    munmap(target, src->size);
    close(file_desc);

    if(src->zipped == 1 && fork() == 0)
        execlp("gunzip", "gunzip", "-f", real_path, NULL);

    free(real_path);

    return src->zipped == 1;
}

/*Extracts the link that is stored in the data chunk whose first block is "block" in the link
specified by the given path.*/
void DataExtractLink(DataBlockId block, char *path){
    File src = DGetDBlockAddress(block);

    if (access(path, F_OK) == 0)
        unlink(path);

    symlink(src->data, path);

    return;
}

/*Wrapper function for DFreeListRemoveLastChunk().*/
void DataRemoveLastChunk(){
    DFreeListRemoveLastChunk();

    return;
}

/*Initializes the data partition. Minimum 1 block needed.
Marks rest of the "blocks-1" blocks as a free chunk.*/
void DataInit(uint64_t blocks){
    DFreeListInit();

    if(blocks > 1)
        DFreeListInsertChunk(1, blocks - 1);

    HeadSetDataSize(blocks << DATA_BLOCK_SHIFT);
    return;
}

/*With this function the "blocks" last blocks of the data partition form a chunk 
which is considered a free chunk.

Useful when truncating in order to add the newly created blocks in the data partition.*/
void DataInsertFreeBlocks(uint64_t blocks){
    if(blocks > 0)
        DFreeListInsertChunk((HeadGetDataSize() >> DATA_BLOCK_SHIFT) - blocks, blocks);

    return;
}