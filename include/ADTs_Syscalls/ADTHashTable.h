#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "ADTList.h"

#pragma once

/*Similar to strdup() but for integers. May be useful for storing integers inside the hash table.*/
uint64_t *intdup(uint64_t x);

//Pointer to hash node. Hash nodes's are hash table's nodes.=
typedef struct hash_node *HashNode;

//Pointer to hash table struct.
typedef struct hash_table *HashTable;

//A function type that takes as arguments a string and the size of an array
//and returns an index to that array.
typedef unsigned int (* HashFunc)(void *, int);

//Given the size of an array, the function hashes the string
//and returns an index.
unsigned int HashString(void *vs, int size);

//Returns the item contained in the hash node.
void *HNGetItem(HashNode node);

//Returns the key contained in the hash node.
void *HNGetKey(HashNode node);

//Creates and initializes a HashTable.
//HashFunc func is responsible for indexing a key
//CompFunc func is used for comparing keys
//DestroyFunc key_destroy is used for destroying keys
//DestroyFunc item_destroy is used for destroying items
HashTable HTCreate(unsigned int size, HashFunc hash, CompFunc comp, DestroyFunc key_destroy, DestroyFunc item_destroy);

//Returns the number of elements that the table holds.
int HTGetSize(HashTable table);

//Returns a pointer to hash node whose key is equal to "key" give according
//to the hash table's comparing function.
//If there is no such hash node, NULL is returned.
HashNode HTFindKey(HashTable table, void *key);

//Replaces from the hash table "table" the item of the 
//hash node with key "key" with the item "item".
//The item being replaced is destroyed.
//Returns 1 if successful and 0 if not.
int HTUpdateKey(HashTable table, void *key, void *item);

//Creates and inserts a hash node with the given key and item.
//If a hash node with an equal key already exists, its item
//is replaced with the given item.
void HTInsertItem(HashTable table, void *key, void *item);

//Removes and destroys the hash node, if it exists, whose key is equal to the given key.
void HTRemoveItem(HashTable table, void *key);

//Destroys the hash given table.
void HTDestroy(HashTable table);