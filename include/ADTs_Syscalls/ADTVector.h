#include "ADTList.h"

typedef void (*DestroyFunc)(void *);

typedef int (*CompFunc)(void *a, void *b);

typedef struct vector *Vector;

//Returns the size of the Vector
int VectorGetSize(Vector vec);

//Creates a Vector of size "size"
Vector VectorCreate(int size, DestroyFunc func);

//Creates a Vector from the list given.
Vector VectorCreateFromList(List list, DestroyFunc func);

//Inserts the element "item" in the last position.
//If there is no room for it, the vector's size is doubled and then
//the insertion is performed.
void VectorInsertLast(Vector vec, void *item);

//Removes the vector's last item. If func != NULL the function
//is used to destroy the item.
void VectorRemoveLast(Vector vec);

//Return the item at the position index of the vector.
//If the index is not in [0, size - 1] NULL is returned.
void *VectorGetAt(Vector vec, int index);

//Destroys the given vector. If func != NULL its items are destroyed
//using the given function.
void VectorReplace(Vector vec, int index, void *item);

//Destroys the given vector. If func != NULL its items are destroyed
//using the given function.
void VectorDestroy(Vector vec);

//Sorts the vector using the Heap Sort algorithm.
//The comparison of the items is performed using the "func" function.
void VectorSort(Vector vec, CompFunc func);
