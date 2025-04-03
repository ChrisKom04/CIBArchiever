#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <linux/limits.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <dirent.h>
#include <libgen.h>
#include <wait.h>

#include "syscalls.h"

#include "header.h"
#include "metadata.h"
#include "data.h"

#include "file_management.h"

#include "cli_utils.h"

#include "ADTList.h"
#include "ADTHashTable.h"

int fd;
void *md = NULL;
void *header = NULL;
void *data = NULL;

/*Compresses the file specified by path and returns the name of the compressed file, which is malloc'd.

Make sure that the given path is a file. This function will not perform wait() to collect the zombie
process that is created due to fork(), exec().*/
char *CIBCompressFile(char *dir_path, char *entry_name){
    int pid = fork();

    if(pid == 0){
        //The name of the gzip-ed file will be the process id of the child process.
        char gzip_name[(32 + strlen(dir_path) + 1)]; 
        snprintf(gzip_name, sizeof(gzip_name), "%s/%d", dir_path, getpid());

        //Createt the path for the target file.
        char file_name[strlen(entry_name) + strlen(dir_path) + 2]; 
        snprintf(file_name, sizeof(file_name), "%s/%s", dir_path, entry_name);

        //Open the gzip file.
        int gzip_fd;
        if(OpenFile(gzip_name, &gzip_fd, O_CREAT | O_RDWR | O_TRUNC, 0666) == -1){
            CIBCannotCompress(file_name);

            //In case of failure it is better to exit the program as the parent process will not know
            //that the file could not be compressed.
            exit(-1);
        }

        //Redirect gzip's fd to stdout and call gzip using execlp.
        DupAndClose(gzip_fd, 1);
        execlp("gzip", "gzip", "-f", "-c", file_name, NULL);
    }

    //Create the name of the gzip-ed file and return it.
    char *gzip_name = malloc(sizeof(char) * 32); snprintf(gzip_name, 32, "%d", pid);
    
    return gzip_name;
}

/*Compresses the files and links under the directory defined by dir and path.

Returns a Hashtable mapping each compressed_file to the original file.*/
HashTable CIBCompressDir(DIR *dir, char *path){
    struct stat cib_info; fstat(fd, &cib_info);
    struct dirent *dir_entry;

    //#Files and Links under directory.
    uint32_t files = 0;
    HashTable mapping = HTCreate(10, HashString, (CompFunc) strcmp, free, free);

    while((dir_entry = readdir(dir)) != NULL){
        
        //Obtain the path of the current entity.
        char entry_path[strlen(path) + strlen(dir_entry->d_name) + 2];
        snprintf(entry_path, sizeof(entry_path), "%s/%s", path, dir_entry->d_name);
        struct stat info; lstat(entry_path, &info);

        //Skip if entry is the .cib file or ".", "..".
        if(strcmp(dir_entry->d_name, ".") == 0 || strcmp(dir_entry->d_name, "..") == 0 || info.st_ino == cib_info.st_ino)
            continue;

        //If entity is file , fork() and compress using exec.
        if((S_ISREG(info.st_mode)) && HTFindKey(mapping, dir_entry->d_name) == NULL){
            HTInsertItem(mapping, CIBCompressFile(path, dir_entry->d_name), strdup(dir_entry->d_name));
            files++;

        }
    }

    rewinddir(dir);
    //Collect the zombie processes.
    for(int i = 0; i < files; i++)
        wait(NULL);

    return mapping;
}

/*Inserts all the entities under the directory that corresponds to the given point. The directory
must be inserted before calling this function and its EntryId has to be passed as a parameter.

If user wants its content compressed then the given flag should be set to true.*/
void CIBInsertDirectory(char *path, EntryId dir_id, bool compress){
    //Obtain the stat info about our cib file. We don't want to include it inside itself.
    struct stat cib_info; fstat(fd, &cib_info);
    
    //Open the directory and create a file-mapping in case user needs the data inserted to be compressed.
    DIR *dir = opendir(path);
    HashTable file_map = compress == true ? CIBCompressDir(dir, path) : NULL;

    //Go through directory entries.
    struct dirent *dir_entry;
    while((dir_entry = readdir(dir)) != NULL){
        //Create the path of the current entry.
        char entry_path[strlen(path) + strlen(dir_entry->d_name) + 2];
        snprintf(entry_path, sizeof(entry_path), "%s/%s", path, dir_entry->d_name);
        //Obtain stat info about the current entry.
        struct stat info; lstat(entry_path, &info);

        //If current entry is ., .. or is the open cib file we skip the loop.
        if(strcmp(dir_entry->d_name, ".") == 0 || strcmp(dir_entry->d_name, "..") == 0 || info.st_ino == cib_info.st_ino)
            continue;

        if(!(S_ISDIR(info.st_mode) || S_ISLNK(info.st_mode) || S_ISREG(info.st_mode))){
            CIBCannotInsertPath(entry_path);

        //If entry is a directory then after inserting it inside the cib file its content is inserted
        //too by calling this function again.
        }else if(S_ISDIR(info.st_mode)){
            CIBEntry entry = CIBEntryCreate(NULL, entry_path); bool inserted; 
            EntryId entry_id = MDUpdatePath(entry, dir_entry->d_name, dir_id, &inserted); free(entry);

            if(inserted == true)
                CIBInsertDirectory(entry_path, entry_id, compress);

        //If user asked for compression and the current entry is a compressed version of an entry of dir
        //we insert it inside cib file.
        }else if(file_map != NULL && HTFindKey(file_map, dir_entry->d_name) != NULL){
            HashNode node = HTFindKey(file_map, dir_entry->d_name);

            char *item = HNGetItem(node);
            char item_path[strlen(item) + strlen(path) + 2]; snprintf(item_path, sizeof(item_path), "%s/%s", path, item);

            CIBEntry entry = CIBEntryCreate(NULL, item_path); bool inserted;
            EntryId entry_id = MDUpdatePath(entry, item, dir_id, &inserted); free(entry);

            if(inserted == true){
                //Pointer is not zero iff an entry with that path already existed. In this case we replace its content.
                if(CIBEntryGetPointer(entry_id) != 0)
                    DataDeleteFile(CIBEntryGetPointer(entry_id));

                DataBlockId block = DataInsertFile(entry_path, true);
                CIBEntrySetPointer(entry_id, block);
            }

            //Delete the gzip file.
            unlink(entry_path);

        //If user does not need compression, or entry is a link, we insert the entry as is.
        }else if(file_map == NULL || S_ISLNK(info.st_mode)){
            CIBEntry entry = CIBEntryCreate(NULL, entry_path); bool inserted;
            EntryId entry_id = MDUpdatePath(entry, dir_entry->d_name, dir_id, &inserted);

            if(inserted == true && CIBEntryIsDir(entry) == true)
                CIBInsertDirectory(entry_path, entry_id, compress);

            else if(inserted == true){
                //Pointer is not zero iff an entry with that path already existed. In this case we replace its content.
                if(CIBEntryGetPointer(entry_id) != 0)
                    DataDeleteFile(CIBEntryGetPointer(entry_id));

                CIBEntrySetPointer(entry_id, CIBEntryIsFile(entry) == true ? DataInsertFile(entry_path, false) : DataInsertLink(entry_path));
            }

            free(entry);
        }
    }

    if(file_map != NULL) HTDestroy(file_map);

    closedir(dir);
    return;
}

/*Inserts each "component" of the given path. For example if this function is called with path
"base_dir/dirA/dirB/file1.txt", where base_dir is the base directory of the cib file, then function
will insert dirA, dirA/dirB, dirA/dirB/dirC.

For each entry inserted with this function its entry id is saved in the hash table for future reference.*/
EntryId CIBRecInsertEntry(char *rel_path, HashTable inserted_entries, bool compress, bool *inserted){
    HashNode node;
    *inserted = true;

    if((node = HTFindKey(inserted_entries, rel_path)) != NULL)
        return *((EntryId *) HNGetItem(node));
    
    char copy[strlen(rel_path) + 1]; strcpy(copy, rel_path);
    char *dir = strdup(dirname(copy)); strcpy(copy, rel_path);

    char *base_name = strdup(basename(copy));
    
    EntryId parent_id = CIBRecInsertEntry(dir, inserted_entries, compress, inserted);

    if(*inserted == false){
        free(dir); free(base_name);
        
        return 0;
    }

    CIBEntry entry = CIBEntryCreate(NULL, rel_path);

    EntryId rel_path_id = MDUpdatePath(entry, base_name, parent_id, inserted);

    if(*inserted == true && compress == true && CIBEntryIsFile(entry) == true){
        if(CIBEntryGetPointer(rel_path_id) != 0)
            DataDeleteFile(CIBEntryGetPointer(rel_path_id));

        char *zipped = CIBCompressFile(dir, base_name); wait(NULL);
        
        DataBlockId block = DataInsertFile(zipped, true);
        CIBEntrySetPointer(rel_path_id, block);

        unlink(zipped);

    }else if(*inserted == true && CIBEntryIsDir(entry) == false){
        if(CIBEntryGetPointer(rel_path_id) != 0)
            DataDeleteFile(CIBEntryGetPointer(rel_path_id));

        DataBlockId block = CIBEntryIsFile(entry) == true ? DataInsertFile(rel_path, false) : DataInsertLink(rel_path);
        CIBEntrySetPointer(rel_path_id, block);

    }else if(*inserted == true && CIBEntryIsDir(entry) == true)
        HTInsertItem(inserted_entries, strdup(rel_path), intdup(rel_path_id));
 
    free(entry); free(dir); free(base_name);
    return rel_path_id;
}

/*Inserts the given paths inside the .cib file. If any of these entries already exist, their content will be updated.

If compressed == true then the data will be compressed before inserted inside the .cib file.*/
void CIBInsertEntries(Vector rel_paths, bool compress){
    HashTable inserted_entries = HTCreate(VectorGetSize(rel_paths) * 2, HashString, (CompFunc) strcmp, free, free);
    HTInsertItem(inserted_entries, strdup("."), intdup(0));

    struct stat cib_info; fstat(fd, &cib_info);

    for(int i = 0; i < VectorGetSize(rel_paths); i++){
        char *path = VectorGetAt(rel_paths, i);
        struct stat info;

        //If attempting to inserted the .cib file itself or an entry that does not exist or is not a file/link/directory
        //then an error message is printed.
        if(lstat(path, &info) == -1 || !(S_ISDIR(info.st_mode) || S_ISLNK(info.st_mode) || S_ISREG(info.st_mode)) || info.st_ino == cib_info.st_ino){
            CIBCannotInsertPath(path);    
            continue;
            
        }

        bool inserted; EntryId entry_id = CIBRecInsertEntry(path, inserted_entries, compress, &inserted);

        if(inserted == true && S_ISDIR(info.st_mode))
            CIBInsertDirectory(path, entry_id, compress);
    }

    HTDestroy(inserted_entries);
    return;
}

//--------------------------------------------

/*Creates the specified cib file and inserted the paths stored in the vector. If compressed == true
then the inserted entities will be compressed before inserttion.*/
void CIBCreate(char *cib_file, Vector paths, bool compress){
    if(OpenFile(cib_file, &fd, O_CREAT | O_RDWR, 0755) == -1)
        return;

    //The cib file will have as a base directory the current working directory. Thus, we make
    //every path given as input relative to the current working directory.
    char *cwd = getcwd(NULL, 0);
    Vector rel_paths = CreateRelativePath(paths, cwd);

    if(VectorGetSize(rel_paths) != 0){
        //Calculate how many node_blocks, data_blocks and list blocks we need.
        uint32_t node_blocks_needed; uint64_t data_blocks;
        uint64_t entries = CalculateSpace(rel_paths, &node_blocks_needed, &data_blocks);
        
        uint32_t list_blocks = entries / LIST_ENTRIES_PER_BLOCK + (entries % LIST_ENTRIES_PER_BLOCK > 0);
        uint64_t md_blocks = 1 + node_blocks_needed + list_blocks; //+1 for the free_list_block.
        uint64_t header_size = HeadCalculateNeededSpace(cwd);
        data_blocks += EXTRA_BLOCKS_NEEDED;

        //Adjust the file size
        TruncMapAndUpdate(header_size, md_blocks * MD_BLOCK_SIZE, data_blocks << DATA_BLOCK_SHIFT, false);
        
        //Initialize each "partition"
        HeadInit(cwd);
        DataInit(data_blocks);
        MDInit(list_blocks, node_blocks_needed);    

        //Insert the entries.
        CIBInsertEntries(rel_paths, compress);

        //Remove, if exist, the unoccupied blocks that make up the last chunk of data size.
        DataRemoveLastChunk();
        CloseExistingCIB();

    }else
        close(fd);

    VectorDestroy(rel_paths); free(cwd);
    return;
}

/*Appends to the existing cib file the paths stored in the given vector. If any inserted entity is already
inserted inside the .cib file then its content is updated.*/
void CIBAppend(char *cib_file, Vector paths, bool compress){
    if(OpenExistingCIB(cib_file) == -1)
        return;

    if(chdir(HeadGetBaseDir()) == -1){
        perror("chdir:");
        return;

    }

    //We make every path relative to the base directory of the .cib file.
    Vector rel_paths = CreateRelativePath(paths, HeadGetBaseDir());
    
    if(VectorGetSize(rel_paths) != 0){
        //Calculate the needed blocks. We care about the data blocks that are generally more
        //than the metadata block that we will need.
        uint32_t node_blocks_needed; uint64_t data_blocks;
        CalculateSpace(rel_paths, &node_blocks_needed, &data_blocks);

        //Adjust the file size.
        TruncMapAndUpdate(HeadGetHeaderSize(), HeadGetMDSize(), HeadGetDataSize() + (data_blocks << DATA_BLOCK_SHIFT), true);
        
        //Move the md partition so that the new space is accessible to the data "partition".
        memmove(md, (char *) md - (data_blocks << DATA_BLOCK_SHIFT), HeadGetMDSize());
        DataInsertFreeBlocks(data_blocks);

        //Update the root entry.
        CIBEntry root = CIBEntryCreate(NULL, "."); bool updated;
        MDUpdatePath(root, ".", 0, &updated); free(root);

        CIBInsertEntries(rel_paths, compress);
        DataRemoveLastChunk();
        CloseExistingCIB();

    }else
        close(fd);

    VectorDestroy(rel_paths);
    return;
}

/*If current_id represents a directory inside .cib file the function calls itself.
If current_id represents a file/link the function extracts it.

The destination of the extracted entities depends on rel_path.

Returns the number of forks() that were made.*/
int CIBExtractRec(char *rel_path, EntryId current_id){

    if(CIBEntryIsDir(GetEntryAddress(current_id)) == true){
        if(CreateDir(rel_path) == -1)
            return -1;

        List entries = MDGetDirEntries(current_id);
        uint64_t count = 0;

        for(LNode node = ListGetFirstNode(entries); node != NULL; node = LNodeGetNext(node)){
            INPair pair = LNodeGetItem(node);

            char *entry_name = INPairGetName(pair);
            EntryId entry_id = INPairGetId(pair);

            char entry_path[strlen(rel_path) + strlen(entry_name) + 2];
            snprintf(entry_path, sizeof(entry_path), "%s/%s", rel_path, entry_name);

            count += CIBExtractRec(entry_path, entry_id);
        }

        ListDestroy(entries);
        for(uint64_t i = 0; i < count; i++)
            wait(NULL);
        
    }else{
        DataBlockId block = CIBEntryGetPointer(current_id);

        if(CIBEntryIsFile(GetEntryAddress(current_id)) == true)
            return DataExtractFile(block, rel_path, 0644) == true;

        else
            DataExtractLink(block, rel_path);
    }

    return 0;
}

/*Extractes the givern paths from the cib_file. Keep in mind that the extracted entities are not deleted
from the cib file and they are still accessible.*/
void CIBExtract(char *cib_file, Vector paths){
    if(OpenExistingCIB(cib_file) == -1)
        return;

    //Number of wait()s that we have to perform in order to collect zombies.
    int waits = 0;

    if(VectorGetSize(paths) == 0)
        waits += CIBExtractRec(".", 0);

    else{
        for(int i = 0; i < VectorGetSize(paths); i++){
            char *path = VectorGetAt(paths, i);

            bool found; EntryId entry_id = MDGetPath(path, 0, &found);

            if(found == true){
                char *copy = strdup(path);
                if(CreateDirRec(dirname(copy)) == -1) continue;

                waits += CIBExtractRec(path, entry_id);
                free(copy);

            }else
                CIBPathNotFound(path, cib_file);
        }

    }

    for(int i = 0; i < waits; i++)
        wait(NULL);

    return;
}

/*Recursively deletes the entity with the current id.

If current id represents a directory then the function calls itself and afterwards the entry is deleted.
If current id represents a file or a link then its data is deleted and afterwards the entry is deleted from
the metadata partition too.*/
void CIBDeleteRec(EntryId current, EntryId parent){

    if(CIBEntryIsDir(GetEntryAddress(current)) == true){
        List entries = MDGetDirEntries(current);

        for(LNode node = ListGetFirstNode(entries); node != NULL; node = LNodeGetNext(node))
            CIBDeleteRec(INPairGetId((INPair) LNodeGetItem(node)), current);
    
        ListDestroy(entries);
    }else{
        DataBlockId pointer = CIBEntryGetPointer(current);

        DataDeleteFile(pointer);
    }

    MDDeleteEntry(current, parent);

    return;
}

/*Deletes the paths stored in the given vector from the .cib file.*/
void CIBDelete(char *cib_file, Vector paths){
    if(OpenExistingCIB(cib_file) == -1)
        return;

    if(chdir(HeadGetBaseDir()) == -1)
        return;

    //We want the paths relative to the base directory of the .cib file.
    Vector rel_paths = CreateRelativePath(paths, HeadGetBaseDir());

    if(VectorGetSize(rel_paths) > 0){    
        for(int i = 0; i < VectorGetSize(rel_paths); i++){
            char *path = VectorGetAt(rel_paths, i);

            //There is no meaning in deleting ".".
            if(strcmp(path, ".") == 0){
                CIBCannotDeleteRoot();
                continue;

            }

            bool found; EntryId current = MDGetPath(path, 0, &found);
            
            if(found == true){
                char *copy = strdup(path);
                char *parent_path = dirname(copy);

                EntryId parent = MDGetPath(parent_path, 0, &found);

                CIBDeleteRec(current, parent);
                free(copy);

            }else
                CIBPathNotFound(path, cib_file);
            
        }

        //If there are blocks at the end of the data partition that are unused then they are deleted.
        DataRemoveLastChunk();
        CloseExistingCIB();
    
    }else{
        close(fd);
        
    }

    VectorDestroy(rel_paths);
    return;
}

/*Prints the structuuure of the given cib file.*/
void CIBPrintStructure(char *cib_file){
    if(OpenExistingCIB(cib_file) == -1)
        return;

    MDPrintStructure();

    CloseExistingCIB();
    return;
}

/*Prints the metadata information about each entity of the cib file.*/
void CIBPrintMetadata(char *cib_file){
    if(OpenExistingCIB(cib_file) == -1)
        return;

    MDPrintEntriesMetadata();

    CloseExistingCIB();
    return;
}

/*Searches the cib to find each path in the Vector. Then, it prints the results of the search.*/
void CIBQuery(char *cib_file, Vector paths){
    if(OpenExistingCIB(cib_file) == -1)
        return;

    char title[512]; snprintf(title, sizeof(title), "+----------------------------+\n| %-26s |\n", "Query Results");
    char *line = "+----------------------------+--------------------------------------------------------------------------------------------------------------------------------+\n";
    
    strcat(title, line);
    WriteBytes(title, strlen(title), 1);

    for(int i = 0; i < VectorGetSize(paths); i++){
        bool found; char *curr = VectorGetAt(paths, i);
        EntryId id = MDGetPath(curr, 0, &found);

        char buff[256 + 4096];
        memset(buff, 0, sizeof(buff));
        if(found == true)
            snprintf(buff, sizeof(buff), "|\033[1;32m EntryID: %17lu \033[0m|\033[1;32m Path: %s\033[0m", id, curr);
        else
            snprintf(buff, sizeof(buff), "|\033[1;31m EntryID: %17s \033[0m|\033[1;31m Path: %s\033[0m", "-", curr);

        for(int i = strlen(buff); i < 180; i++) 
            buff[i] = ' ';

        strcat(buff, "|\n");
        WriteBytes(buff, strlen(buff), 1);
        WriteBytes(line, strlen(line), 1);
    }

    CloseExistingCIB();

    return;
}

/*Selects which function should be called.*/
void CIBStart(CIBArgs args){
    switch(args->flags){
        case C: CIBCreate(args->cib_file, args->paths, false); break;
        case C | J: CIBCreate(args->cib_file, args->paths, true); break;
        case A: CIBAppend(args->cib_file, args->paths, false); break;
        case A | J: CIBAppend(args->cib_file, args->paths, true); break;
        case D: CIBDelete(args->cib_file, args->paths); break;
        case Q: CIBQuery(args->cib_file, args->paths); break;
        case X: CIBExtract(args->cib_file, args->paths); break;
        case M: CIBPrintMetadata(args->cib_file); break;
        case P: CIBPrintStructure(args->cib_file); break;
        default: break;
    }

    return;
}

int main(int argc, char *argv[]){
    CIBArgs args;
    
    if((args = CIBReadArgs(argc, argv)) == NULL)
        return -1;

    CIBStart(args);

    CIBArgsDestroy(args);
    return 0;
}