#pragma once

//Pointer to struct list.
typedef struct list *List;

//Pointer to struct list_ndoe. A list_node is a list's node.
typedef struct list_node *LNode;

//A function that returns 0 if two objects are equal.
typedef int (* CompFunc)(void *, void *);

//A function responsible to free the space an object is using.
typedef void (* DestroyFunc)(void *);

//Returns the item contained inside the node.
void *LNodeGetItem(LNode node);

//Returns the following node. If there is no list node after "node" NULL is returned.
LNode LNodeGetNext(LNode node);

//Returns the previous node. If there is no list node before "node" NULL is returned.
LNode LNodeGetPrevious(LNode node);

//Creates an empty list and returns a pointer to it.
List ListCreate(DestroyFunc func);

//Destroys the given list.
//If func != NULL the items that the list's nodes contain are also destroyed, using the given function.
void ListDestroy(List list);

//Returns the first node of the list. If the list is empty NULL is returned.
LNode ListGetFirstNode(List list);

//Returns the last node of the list. If the list is empty NULL is returned.
LNode ListGetLastNode(List list);

//Returns the size of the List given.
int ListGetSize(List list);

//Creates a list node that contains the item given and inserts that node in the begining of the list.
void ListInsertFirst(List list, void *item);

//Creates a list node that contains the item given and inserts that node in the end of the list.
void ListInsertLast(List list, void *item);

//Goes through the List given and returns a pointer to the first node, whose content
//is equal to "item" according to the "comp" function. If there is no such node NULL is returned.
//CompFunc given will be used with "item" as its second argument.
LNode ListFindItem(List list, void *item, CompFunc comp);

//Destroys LNode "node" and removes it from the list.
//If DestroyFunc "func" != NULL, the item contained by the node is destroyed.
//Undefined behaviour if "node" is not part of the list given.
void ListRemoveNode(List list, LNode node);

//Appends list2 at the end of list1.
List ListAppend(List list1, List list2);