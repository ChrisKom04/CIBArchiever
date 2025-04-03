#include <stdint.h>

#include "ADTVector.h"
#include "metadata.h"
#include "cib_struct.h"

typedef struct entry_path_pair{
    char *path;
    CIBEntry entry;

}* EPPair;


void CIBCreate(char *cib_file, Vector paths);
void CIBPrintStructure(char *cib_file);
void CIBPrintMetadata(char *cib_file);
void CIBQuery(char *cib_file, Vector paths);

void TruncMapAndUpdate(uint64_t header_size, int64_t md_size, int64_t data_size, bool mapped);

void DataRemoveLastChunk();