#include "ADTHashTable.h"

#define START_CAP 13

//Struct used for a node of a Hash Table
typedef struct hash_node{
    void *key;
    void *item;

}* HashNode;

//Struct used for the Hash Table
typedef struct hash_table{
    List *array;
    unsigned int capacity;      //size of the array.
    unsigned int size;          //number of items inside the hash table.
    HashFunc hash;              //function used to find index for a key.
    CompFunc comp;              //function to compare keys.
    DestroyFunc keyDestroy;     //function responsible for destroying a key.
    DestroyFunc itemDestroy;    //function responsible for destroying an item.

}* HashTable;

//Given the size of an array, the function hashes the string
//and returns an index.
unsigned int HashString(void *vs, int size){
    char *s = vs;
    unsigned long int res = 0, a = 3727;
    
    for(; *s != 0; s++)
        res = (res * a + (unsigned char) *s) % size;

    return (int) res;
}

/*Similar to strdup() but for integers. May be useful for storing integers inside the hash table.*/
uint64_t *intdup(uint64_t x){
    uint64_t *c = malloc(sizeof(uint64_t));
    *c = x;

    return c;
}

//----------------------------------------! Hash Node !----------------------------------------

//Creates a hash node that contains "key" and "item" as its key and item.
HashNode HNCreate(void *key, void *item){
    HashNode node = malloc(sizeof(struct hash_node));
    node->key = key;
    node->item = item;

    return node;
}

//Returns the item contained in the hash node.
void *HNGetItem(HashNode node){
    return node->item;

}

//Returns the keu contained in the hash node.
void *HNGetKey(HashNode node){
    return node->key;

}

//Destroys the hash node given.
//If DestroyFunc "keyDestroy" != NULL, the key contained is destroyed.
//Same applies for DestroyFunc "itemDestroy".
void HNDestroy(HashNode node, DestroyFunc keyDestroy, DestroyFunc itemDestroy){
    if(itemDestroy != NULL)
        itemDestroy(node->item);

    if(keyDestroy != NULL)
        keyDestroy(node->key);

    free(node);
    return;
}



//----------------------------------------! Hash Table !----------------------------------------

//Creates and initializes a HashTable.
//HashFunc func is responsible for indexing a key
//CompFunc func is used for comparing keys
//DestroyFunc key_destroy is used for destroying keys
//DestroyFunc item_destroy is used for destroying items
HashTable HTCreate(unsigned int size, HashFunc func, CompFunc comp, DestroyFunc key_destroy, DestroyFunc item_destroy){
    HashTable map = malloc(sizeof(struct hash_table));
    map->capacity = 1;
    while(map->capacity < 2 * size)
        map->capacity = map->capacity << 1;
    
    map->capacity = 2 * size;
    map->size = 0;
    map->hash = func;
    map->comp = comp;
    map->keyDestroy = key_destroy;
    map->itemDestroy = item_destroy;

    map->array = malloc(map->capacity * sizeof(List));
    memset(map->array, 0, map->capacity * sizeof(List));    //We want for start every cell to contain NULL pointer
    
    return map;
}

int HTGetSize(HashTable table){
    return table->size;
}

//Inserts hash node into the hash table.
void HTInsertNode(HashTable table, HashNode node){
    //Calculates the appropriate index
    unsigned int index = table->hash(node->key, table->capacity);

    //If there is no list in the cell, one is created.
    if(table->array[index] == NULL)
        table->array[index] = ListCreate(NULL);

    ListInsertFirst(table->array[index], node);
    return;
}

//Given a list containing pointers to hash nodes, the function returns
//the first list node in the list that contains a hash node with a key equal to "key".
//If there is no such list node, NULL is returned.
LNode FindKeyInList(HashTable table, List list, void *key){ 
    for(LNode iter = ListGetFirstNode(list); iter != NULL; iter = LNodeGetNext(iter)){
        HashNode hnode = LNodeGetItem(iter);

        if(table->comp(hnode->key, key) == 0)
            return iter;
    }

    return NULL;
}

//Returns a pointer to hash node whose key is equal to "key" give according
//to the hash table's comparing function.
//If there is no such hash node, NULL is returned.
HashNode HTFindKey(HashTable table, void *key){
    //Calculates the appropriate index
    unsigned int index = table->hash(key, table->capacity);

    //Searches for the list in the specific index, if it exists,
    //for a hash node with an equal key.
    if(table->array[index] != NULL){
        LNode node = FindKeyInList(table, table->array[index], key);
        return node != NULL ? LNodeGetItem(node) : NULL;
        
    }

    return NULL;
}

//Replaces from the hash table "table" the item of the 
//hash node with key "key" with the item "item".
//The item being replaced is destroyed.
//Returns 1 if successful and 0 if not.
int HTUpdateKey(HashTable table, void *key, void *item){
    HashNode node = HTFindKey(table, key);

    if(node != NULL){
        if(item != node->item && table->itemDestroy != NULL)
            table->itemDestroy(node->item);

        node->item = item;
    }

    return node != NULL;
}

//Creates and inserts a hash node with the key and item given.
//If a hash node with an equal key already exists, its item
//is replaced with the item given.
void HTInsertItem(HashTable table, void *key, void *item){
    if(HTUpdateKey(table, key, item) == 0){
        HashNode node = HNCreate(key, item);
        HTInsertNode(table, node);

        table->size++;
    }

    return;
}

//Removes and destroys the hash node, if it exists, whose key is equal to the key given.
void HTRemoveItem(HashTable table, void *key){
    //Calculates the appropriate index
    unsigned int index = table->hash(key, table->capacity);

    if(table->array[index] != NULL){
        LNode target = FindKeyInList(table, table->array[index], key);

        if(target != NULL){
            HashNode node = LNodeGetItem(target);
            HNDestroy(node, table->keyDestroy, table->itemDestroy);

            //Removes the pointer to the hash node from the adjacency list.
            ListRemoveNode(table->array[index], target);
            table->size--;
            
        }
    }

    return;
}

//Destroys the hash table given.
void HTDestroy(HashTable table){
    for(unsigned int i = 0; i < table->capacity; i++){
        if(table->array[i] != NULL){

            //Destroys the hash nodes as well as the list containing pointers to them.
            for(LNode node = ListGetFirstNode(table->array[i]); node != NULL; node = LNodeGetNext(node)){
                HashNode hnode = LNodeGetItem(node);
                HNDestroy(hnode, table->keyDestroy, table->itemDestroy);

            }

            ListDestroy(table->array[i]);
        }
    }

    free(table->array);
    free(table);
    return;
}