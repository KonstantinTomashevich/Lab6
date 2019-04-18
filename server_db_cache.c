#include "server_db_cache.h"
#include <stdio.h>
#include <CContainers/PHashMap.h>
#include <CContainers/PDoubleLinkedList.h>
#include <CContainers/Utils.h>

typedef struct
{
    int id;
    unsigned int readersCount;
    DWORD writer;
} SyncRecord;

#define NO_WRITER -1
#define MAX_SYNC_RECORDS 100

int unusedSyncRecords;
char *chServerDatabase;
PHashMapHandle phmSyncRegistry;
PDoubleLinkedListHandle pdllSyncRecordsDateOrder;
HANDLE hRegistryMutex;

static ulint Callback_HashKey (void *key)
{
    return (ulint) key;
}

static lint Callback_KeyCompare (void *first, void *second)
{
    return ((ulint) first) - ((ulint) second);
}

void InitServerDBCache (char *chServerDatabaseFileName)
{
    chServerDatabase = chServerDatabaseFileName;
    phmSyncRegistry = PHashMap_Create (100, 5, Callback_HashKey, Callback_KeyCompare);
    pdllSyncRecordsDateOrder = PDoubleLinkedList_Create ();
    hRegistryMutex = CreateMutex (NULL, FALSE, NULL);
    unusedSyncRecords = 0;
}

void DestructServerDBCache ()
{
    PHashMap_Destruct (phmSyncRegistry, ContainerCallback_NoAction, ContainerCallback_Free);
    PDoubleLinkedList_Destruct (pdllSyncRecordsDateOrder, ContainerCallback_NoAction);
}

static void SyncRecordEdition_TryFixSyncRecordsCount ()
{
    PDoubleLinkedListIterator iterator = PDoubleLinkedList_Begin (pdllSyncRecordsDateOrder);
    while (PHashMap_Size (phmSyncRegistry) >= MAX_SYNC_RECORDS && unusedSyncRecords != 0 &&
        iterator != PDoubleLinkedList_End (pdllSyncRecordsDateOrder))
    {
        SyncRecord *data = *(SyncRecord **) PDoubleLinkedListIterator_ValueAt (iterator);
        if (data->readersCount > 0 || data->writer != NO_WRITER)
        {
            iterator = PDoubleLinkedListIterator_Next (iterator);
        }
        else
        {
            --unusedSyncRecords;
            iterator = PDoubleLinkedList_Erase (pdllSyncRecordsDateOrder, iterator);
            ulint key = data->id;
            PHashMap_Erase (phmSyncRegistry, (void *) key, ContainerCallback_NoAction, ContainerCallback_Free);
        }
    }

    printf ("%ld /u%d\n", PHashMap_Size (phmSyncRegistry), unusedSyncRecords);
}

static SyncRecord *SyncRecordEdition_GetRecordSyncData (int id)
{
    ulint key = id;
    if (PHashMap_ContainsKey (phmSyncRegistry, (void *) key))
    {
        return (SyncRecord *) *PHashMap_GetValue (phmSyncRegistry, (void *) key);
    }
    else
    {
        SyncRecordEdition_TryFixSyncRecordsCount ();
        SyncRecord *data = malloc (sizeof (SyncRecord));

        data->id = id;
        data->readersCount = 0;
        data->writer = NO_WRITER;

        PHashMap_Insert (phmSyncRegistry, (void *) key, data);
        PDoubleLinkedList_Insert (pdllSyncRecordsDateOrder, PDoubleLinkedList_End (pdllSyncRecordsDateOrder), data);
        ++unusedSyncRecords;
        return data;
    }
}

BOOL RequestModifyRecord (int id)
{
    WaitForSingleObject (hRegistryMutex, INFINITE);
    SyncRecord *data = SyncRecordEdition_GetRecordSyncData (id);

    if (data->readersCount > 0 || data->writer != NO_WRITER)
    {
        ReleaseMutex (hRegistryMutex);
        return FALSE;
    }

    data->writer = GetCurrentThreadId ();
    --unusedSyncRecords;
    ReleaseMutex (hRegistryMutex);
    return TRUE;
}

BOOL TryProcessReadCommand (int id, TaxPayment *output)
{
    WaitForSingleObject (hRegistryMutex, INFINITE);
    SyncRecord *data = SyncRecordEdition_GetRecordSyncData (id);

    if (data->writer != NO_WRITER)
    {
        ReleaseMutex (hRegistryMutex);
        return FALSE;
    }

    if (data->readersCount == 0)
    {
        --unusedSyncRecords;
    }

    data->readersCount++;
    ReleaseMutex (hRegistryMutex);

    FILE *db = fopen (chServerDatabase, "rb");
    fseek (db, id * sizeof (TaxPayment), SEEK_SET);
    fread (output, sizeof (TaxPayment), 1, db);
    fclose (db);

    WaitForSingleObject (hRegistryMutex, INFINITE);
    data->readersCount--;

    if (data->readersCount == 0)
    {
        ++unusedSyncRecords;
    }

    ReleaseMutex (hRegistryMutex);
    return TRUE;
}

BOOL ProcessModifyCommand (TaxPayment *newValue)
{
    WaitForSingleObject (hRegistryMutex, INFINITE);
    SyncRecord *data = SyncRecordEdition_GetRecordSyncData (newValue->num);

    if (data->writer != GetCurrentThreadId ())
    {
        ReleaseMutex (hRegistryMutex);
        return FALSE;
    }

    ReleaseMutex (hRegistryMutex);

    FILE *db = fopen (chServerDatabase, "rb+");
    fseek (db, newValue->num * sizeof (TaxPayment), SEEK_SET);
    fwrite (newValue, sizeof (TaxPayment), 1, db);
    fclose (db);

    WaitForSingleObject (hRegistryMutex, INFINITE);
    data->writer = NO_WRITER;
    ++unusedSyncRecords;
    ReleaseMutex (hRegistryMutex);
    return TRUE;
}
