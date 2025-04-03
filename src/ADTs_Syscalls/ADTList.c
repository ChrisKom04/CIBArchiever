#include "ADTList.h"
#include <stdlib.h>

//Struct used for a node of a list.
//Contains an item and pointers to the next and the previous nodes of a list.
struct list_node{
    void *item;
    LNode next;
    LNode previous;

};

//Struct used for the list.
struct list{
    LNode head;
    LNode tail;
    int size;

    DestroyFunc func;
};

//----------------------------------------! List Node !----------------------------------------

//Creates a list_node that contains "pointer", has "next" as its next list_node and "previous" as its
//previous list_node.
LNode LNodeCreate(void *pointer, LNode next, LNode previous){
    LNode node = malloc(sizeof(struct list_node));
    node->item = pointer;
    node->next = next;
    node->previous = previous;

    return node;
}

//Destroys list_node.
//If func != NULL the function given is called to destroy the item inside list_node.
void LNodeDestroy(LNode node, DestroyFunc func){
    if(func != NULL){func(node->item);}
    free(node);

}

//Returns the following node. If there is no list_node after "node" NULL is returned.
LNode LNodeGetNext(LNode node){
    return node->next;

}

//Returns the previous node. If there is no list_node before "node" NULL is returned.
LNode LNodeGetPrevious(LNode node){
    return node->previous;

}

//Returns the item contained inside the node.
void *LNodeGetItem(LNode node){
    return node->item;

}

//----------------------------------------! List !----------------------------------------

//Creates an empty list and returns a pointer to it.
List ListCreate(DestroyFunc func){
    List list = malloc(sizeof(struct list));
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    list->func = func;

    return list;
}

//Destroys the given list.
//If func != NULL the items that the list's nodes contain are also destroyed, using the given function.
void ListDestroy(List list){
    LNode iter = ListGetFirstNode(list);
    while(iter != NULL){
        LNode next = iter->next;
        LNodeDestroy(iter, list->func);
        
        iter = next;
    }

    free(list);
    return;
}

//Returns the first node of the list. If the list is empty NULL is returned.
LNode ListGetFirstNode(List list){
    return list->head;

}

//Returns the last node of the list. If the list is empty NULL is returned.
LNode ListGetLastNode(List list){
    return list->tail;

}

//Returns the size of the List given.
int ListGetSize(List list){
    return list->size;

}

//Creates a list node that contains the item given and inserts that node in the begining of the list.
void ListInsertFirst(List list, void *item){
    LNode node = LNodeCreate(item, list->head, NULL);

    if(list->size++ == 0)
        list->tail = node;

    else
        list->head->previous = node;


    list->head = node;
    return;
}

//Creates a list node that contains the item given and inserts that node in the end of the list.
void ListInsertLast(List list, void *item){
    LNode node = LNodeCreate(item, NULL, list->tail);
    
    if(list->size++ == 0)
        list->head = node;
    else
        list->tail->next = node;

    list->tail = node;
    return;
}

//Goes through the List given and returns a pointer to the first node, whose content
//is equal to "item" according to the "comp" function. If there is no such node NULL is returned.
//CompFunc given will be used with "item" as its second argument.
LNode ListFindItem(List list, void* item, CompFunc comp){
    for(LNode i = list->head; i != NULL; i = i->next)
        if(comp(i->item, item) == 0)
            return i;

    return NULL;

}

//Destroys LNode "node" and removes it from the list.
//If DestroyFunc "func" != NULL, the item contained by the node is destroyed.
//Undefined behaviour if "node" is not part of the list given.
void ListRemoveNode(List list, LNode node){
    if(list->size-- == 1){
        list->head = NULL;
        list->tail = NULL;

    }else if(node == list->head){
        list->head = node->next;
        list->head->previous = NULL;

    }else if(node == list->tail){
        list->tail = node->previous;
        list->tail->next = NULL;

    }else{
        node->previous->next = node->next;
        node->next->previous = node->previous;

    }

    LNodeDestroy(node, list->func);
    return;

}

//Appends list2 at the end of list1.
List ListAppend(List list1, List list2){
    
    if(ListGetSize(list1) == 0){
        list1->head = list2->head;
        list1->tail = list2->tail;
        list1->size = list2->size;

        free(list2);
        return list1;

    }else if(ListGetSize(list2) == 0){
        
        free(list2);
        return list1;
    }

    list1->size += list2->size;

    list1->tail->next = list2->head;
    list2->head->previous = list1->tail;
    
    list1->tail = list2->tail;
    
    free(list2);
    return list1;
}