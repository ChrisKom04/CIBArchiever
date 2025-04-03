#include <stdlib.h>
#include "ADTVector.h"

int *HeapInt(int x){
    int *p = malloc(sizeof(int)); *p = x;

    return p;
}

//Struct vector.
typedef struct vector{
    void **array;

    int size;       //the number of elements inside the vector
    int capacity;   //the number of elements it can hold

    DestroyFunc func;
}*Vector;

//Returns the size of the Vector
int VectorGetSize(Vector vec){
    return vec->size;
}

//Creates a Vector of size 2 * size.
Vector VectorCreate(int size, DestroyFunc func){
    Vector vec = malloc(sizeof(struct vector));

    vec->capacity = size << 1;
    vec->size = 0;
    vec->array = malloc(sizeof(void *) * vec->capacity);
    vec->func = func;

    return vec;
}

//Creates a Vector from the list given.
Vector VectorCreateFromList(List list, DestroyFunc func){
    Vector vec = VectorCreate(ListGetSize(list), func);

    for(LNode iter = ListGetFirstNode(list); iter != NULL; iter = LNodeGetNext(iter))
        vec->array[vec->size++] = LNodeGetItem(iter);

    return vec;
}

//Inserts the element item in the last position.
//If there is no room for it the vector's size is doubled and then
//the insertion is performed.
void VectorInsertLast(Vector vec, void *item){
    if(vec->size == vec->capacity){

        vec->array = realloc(vec->array, vec->capacity * 2 * sizeof(void *));
        vec->capacity <<= 1;

    }

    vec->array[vec->size] = item;
    vec->size++;

    return;
}

//Removes the vector's last item. If func != NULL the function
//is used to destroy the item.
void VectorRemoveLast(Vector vec){
    if(vec->func != NULL)
        vec->func(vec->array[vec->size - 1]);

    vec->size--;
    return;
}

//Return the item at the position index of the vector.
//If the index is not in [0, size - 1] NULL is returned.
void *VectorGetAt(Vector vec, int index){
    if(index < 0 || index >= vec->size)
        return NULL;

    return vec->array[index];
}

//Replaces the item in the position "index" with "item"
void VectorReplace(Vector vec, int index, void *item){
    if(index < 0 || index >= vec->size)
        return;

    vec->array[index] = item;
    return;
}

//Destroys the given vector. If func != NULL its items are destroyed
//using the given function.
void VectorDestroy(Vector vec){
    if(vec->func != NULL){
        for(int i = 0; i < vec->size; i++)
            vec->func(vec->array[i]);
            
    }
    free(vec->array);
    free(vec);

    return;
}

//Recursive function used by Heap Sort.
void HeapSortRec(Vector vec, int start, int end, CompFunc func){
    if(end - start == 0)
        return;

    int middle = start + (end - start)/2;
    HeapSortRec(vec, start, middle, func);
    HeapSortRec(vec, middle + 1, end, func);

    Vector temp = VectorCreate(end - start + 1, NULL);
    for(int i = 0, j = start, k = middle + 1; i < end - start + 1; i++){

        if(k > end)
            VectorInsertLast(temp, vec->array[j++]);

        else if(j > middle)
            VectorInsertLast(temp, vec->array[k++]);

        else
            VectorInsertLast(temp, func(vec->array[j], vec->array[k]) > 0 ? vec->array[j++] : vec->array[k++]);

    }

    for(int i = 0; i < temp->size; i++)
        vec->array[start + i] = temp->array[i];

    VectorDestroy(temp);
    return;
}

//Sorts the vector using the Heap Sort algorithm.
//The comparison of the items is performed using the "func" function.
void VectorSort(Vector vec, CompFunc func){
    if(func == NULL || vec->size == 0)
        return;

    HeapSortRec(vec, 0, vec->size - 1, func);
    return;
}