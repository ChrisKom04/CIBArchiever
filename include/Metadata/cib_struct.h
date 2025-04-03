#include <stdbool.h>
#include "metadata.h"
#include "ADTList.h"

/*Initializes the given list block.*/
void CIBListBlockInit(ListBlock block);

/*Initializes the CIBList.*/
void CIBListInit(CIBEntry root);

/*Returns a list containing the pairs <entry_id, name> that the directory specified by the
entry_id contains. If the given entry_id is not a directory, NULL is returned.*/
List CIBListGetDirEntries(EntryId dir_id);

/*Prints the Metadata for each entry stored in the metadata partition.*/
void CIBListPrintEntriesMetadata();

/*Prints the struct of the directory with id "current_id" and name "name". This funtion
is also called on its subdirectories.*/
void CIBListPrintStructure(EntryId current_id, char *name);

/*Inserts the first "component" of the given path that does not exist insinde the cib file.

If no such "component" exists then the last "component" of the path is updated.
We consider the path to be relative to the directory defined by the entry id "current_id".*/
EntryId CIBListUpdateEntry(CIBEntry entry, char *path, EntryId current_id, bool *inserted);

/*Returns the entry id of the entry defined by the path given, which is relative to the path
of the directory specified by the "current_id".

Found is set to true if the requested entry was found. Otherwise it is set to false.*/
EntryId CIBListGetEntry(EntryId current_id, char *path, bool *found);

/*Deletes the entry specified by entry_id. If it is a directory then its content
is also deleted.*/
void CIBListDeleteEntry(EntryId entry_id, EntryId parent_id);