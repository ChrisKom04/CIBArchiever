#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "cli_utils.h"
#include "syscalls.h"

//------------------------------------------------------------------
//Error Messages

/*Error Message: Path is not a directory.*/
void CIBPathIsNotDir(char *path){
    char buff[128 + strlen(path)];
    snprintf(buff, sizeof(buff), "./cib: Error: Path %s already exists and is not a directory.\n", path);

    WriteBytes(buff, strlen(buff), 2);
    return;
}

/*Error Message: Cannot Delete ".".*/
void CIBCannotDeleteRoot(){
    WriteBytes("./cib: Error: Cannot delete '.'.\n", 33, 2);

    return;
}

/*Error Message: Path not found inside .cib file.*/
void CIBPathNotFound(char *path, char *cib_file){
    char buff[64 + strlen(path)];
    snprintf(buff, sizeof(buff), "./cib: Error: Path %s does not exist inside %s file.\n", path, cib_file);

    WriteBytes(buff, strlen(buff), 2);
    return;
}

/*Error Message: Path cannot be inserted.*/
void CIBCannotInsertPath(char *path){
    char buff[128 + strlen(path)];
    snprintf(buff, sizeof(buff), "./cib: Error: Entity defined by path %s cannot be inserted.\n", path);

    WriteBytes(buff, strlen(buff), 2);
    return;
}

/*Error Message: Path cannot be compressed.*/
void CIBCannotCompress(char *path){
    char buff[64 + strlen(path)];
    snprintf(buff, sizeof(buff), "./cib: Error: Cannot compress file %s. Insertion Failed.\n", path);

    WriteBytes(buff, strlen(buff), 2);
    return;
}

/*Error Message: Path does not exist.*/
void CIBPathDoesNotExist(char *path){
    char buff[64 + strlen(path)]; 
    snprintf(buff, sizeof(buff), "./cib: Error: Path %s does not exist.\n", path);

    WriteBytes(buff, strlen(buff), 2);
    return;
}

/*Returns the full path. Function assumes that path is relative to cwd.*/
char *FullPath(const char *path){
    char *cwd = getcwd(NULL, 0), *full_path;
    
    if(path[0] != '/'){
        full_path = malloc(sizeof(char) * (2 + strlen(path) + strlen(cwd)));
        
        strcpy(full_path, cwd);
        strcat(full_path, "/");
        strcat(full_path, path);

    }else{
        full_path = malloc(sizeof(char) * (1 + strlen(path)));

        strcpy(full_path, path);
    }

    free(cwd);
    return full_path;
}

/*Removes "." and ".." out of a path. Returns a pointer to the new path.*/
char *RealPath(const char *path) {
    char *full_path = FullPath(path);
    char *output = calloc(strlen(full_path) + 1, sizeof(char));
    List temp = ListCreate(NULL);
    
    char *ch = NULL;
    while((ch = strtok(ch == NULL ? full_path : NULL, "/")) != NULL)
        ListInsertLast(temp, ch);

    for(LNode node = ListGetFirstNode(temp); node != NULL;){
        char *item = LNodeGetItem(node);

        if(strcmp(item, "..") == 0){
            LNode next_node = LNodeGetNext(node);            
            ListRemoveNode(temp, LNodeGetPrevious(node));
            ListRemoveNode(temp, node);

            node = next_node;
            continue;
            
        }else if(strcmp(item, ".") == 0){
            LNode next_node = LNodeGetNext(node);
            ListRemoveNode(temp, node);

            node = next_node;
            continue;
        }
        
        node = LNodeGetNext(node);
    }

    for(LNode node = ListGetFirstNode(temp); node!= NULL; node = LNodeGetNext(node)){
        
        strcat(output, "/");
        strcat(output, (char *) LNodeGetItem(node));
    }

    if(strlen(output) == 0)
        strcpy(output, "/");

    ListDestroy(temp);
    free(full_path);
    
    return output;
}


/*Given a vector of paths, which may be full paths or relative to cwd, this function
makes them relative to the given base_dir.

The given vector remains intact. Returns a vector containing the relative paths.*/
Vector CreateRelativePath(Vector paths, char *base_dir){
    Vector full_paths = VectorCreate(VectorGetSize(paths), free);
    char *cwd = getcwd(NULL, 0);

    for(int i = 0; i < VectorGetSize(paths); i++){
        char *full_path = RealPath(VectorGetAt(paths, i));

        int base_len = strlen(base_dir), full_len = strlen(full_path);

        if(base_len > full_len || strncmp(base_dir, full_path, strlen(base_dir)) || (full_path[base_len] != 0 && full_path[base_len] != '/')){

            char buff[base_len + full_len + 29];
            snprintf(buff, sizeof(buff), "%s is not a subdirectory of %s\n", full_path, base_dir);

            WriteBytes(buff, strlen(buff), 2);
        
        }else{
            char *relative_path = full_len <= base_len + 1 ? strdup(".") : strdup(full_path + base_len + 1);
            VectorInsertLast(full_paths, relative_path);
        }

        free(full_path);
    }

    free(cwd);
    return full_paths;
}

/*Frees the memory of cib_arguments struct.*/
void CIBArgsDestroy(CIBArgs args){
    VectorDestroy(args->paths);
    
    free(args->cib_file);
    free(args);

    return;
}

/*Reads the arguments and stores them inside a cib_arguments struct.

If the input was correct a pointer to the cib_arguments struct is returned.
Otherwise, NULL is returned.*/
CIBArgs CIBReadArgs(int argc, char *argv[]){
    CIBArgs  arguments = calloc(1, sizeof(struct cib_arguments));
    arguments->paths = VectorCreate(argc, free);
    
    bool flag = false;
    
    for(int i = 1; i < argc && flag == false; i++){
        if(argv[i][0] == '-' && strlen(argv[i]) == 2){

            switch (argv[i][1]){
                case 'c': arguments->flags |= C; break;
                case 'a': arguments->flags |= A; break;
                case 'x': arguments->flags |= X; break;
                case 'j': arguments->flags |= J; break;
                case 'd': arguments->flags |= D; break;
                case 'm': arguments->flags |= M; break;
                case 'q': arguments->flags |= Q; break;
                case 'p': arguments->flags |= P; break;
                default: flag = true;
            }
            
        }else if(argv[i][0] != '-' && arguments->cib_file == NULL){
            arguments->cib_file = strdup(argv[i]);

        }else if(argv[i][0] != '-'){
            VectorInsertLast(arguments->paths, strdup(argv[i]));

        }else{
            flag = true;

        }
    }

    switch (arguments->flags){
        case C: case A: case X: case D: case M:
        case Q: case P: case C | J: case A | J: break;

        default: flag = true;
    }

    if(flag == true || !((VectorGetSize(arguments->paths) == 0 && arguments->flags >= X) || (VectorGetSize(arguments->paths) > 0 && arguments->flags <= X))){
        char *error_msg = "cib: Error: Missing or Invalid arguments.\n\
Usage:\n\
    cib -c <archive-file> <list-of-files/dirs>     Create a new archive.\n\
        -a <archive-file> <list-of-files/dirs>     Append files/directories to an existing archive.\n\
        -x <archive-file> <list-of-files/dirs>     Extract the contents to the current directory. List may be empty.\n\
        -j <archive-file>                          Compress the archive using gzip. Used only with -a or -c\n\
        -d <archive-file> <list-of-files/dirs>     Delete files/directories from the archive.\n\
        -m <archive-file>                          Print metadata of stored items.\n\
        -q <archive-file> <list-of-files/dirs>     Check if files/directories exist in the archive.\n\
        -p <archive-file>                          Print a human-readable archive structure.\n";


        WriteBytes(error_msg, strlen(error_msg), 2);

        VectorDestroy(arguments->paths);
        free(arguments->cib_file);
        free(arguments);
        return NULL;
    }

    return arguments;
}