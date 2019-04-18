#include "server_db_cache.h"
#include <stdio.h>
#include <CContainers/PHashMap.h>
#include <CContainers/Utils.h>

typedef struct
{
    unsigned int readersCount;
    DWORD writer;
} RecordSyncData;

#define NO_WRITER -1
char *chServerDatabase;
PHashMapHandle phmSyncRegistry;
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
    hRegistryMutex = CreateMutex (NULL, FALSE, NULL);
}

static void Callback_FreeRecordSyncData (void **value)
{
    free (*value);
}

void DestructServerDBCache ()
{
    PHashMap_Destruct (phmSyncRegistry, ContainerCallback_Free, Callback_FreeRecordSyncData);
}

static RecordSyncData *GetRecordSyncData (int id)
{
    ulint key = id;
    if (PHashMap_ContainsKey (phmSyncRegistry, (void *) key))
    {
        return (RecordSyncData *) *PHashMap_GetValue (phmSyncRegistry, (void *) key);
    }
    else
    {
        RecordSyncData *data = malloc (sizeof (RecordSyncData));
        data->readersCount = 0;
        data->writer = NO_WRITER;

        PHashMap_Insert (phmSyncRegistry, (void *) key, data);
        return data;
    }
}

BOOL RequestModifyRecord (int id)
{
    WaitForSingleObject (hRegistryMutex, INFINITE);
    RecordSyncData *data = GetRecordSyncData (id);

    if (data->readersCount > 0 || data->writer != NO_WRITER)
    {
        ReleaseMutex (hRegistryMutex);
        return FALSE;
    }

    data->writer = GetCurrentThreadId ();
    ReleaseMutex (hRegistryMutex);
    return TRUE;
}

BOOL TryProcessReadCommand (int id, TaxPayment *output)
{
    WaitForSingleObject (hRegistryMutex, INFINITE);
    RecordSyncData *data = GetRecordSyncData (id);

    if (data->writer != NO_WRITER)
    {
        ReleaseMutex (hRegistryMutex);
        return FALSE;
    }

    data->readersCount++;
    ReleaseMutex (hRegistryMutex);

    FILE *db = fopen (chServerDatabase, "rb");
    fseek (db, id * sizeof (TaxPayment), SEEK_SET);
    fread (output, sizeof (TaxPayment), 1, db);
    fclose (db);

    WaitForSingleObject (hRegistryMutex, INFINITE);
    data->readersCount--;
    ReleaseMutex (hRegistryMutex);
    return TRUE;
}

BOOL ProcessModifyCommand (TaxPayment *newValue)
{
    WaitForSingleObject (hRegistryMutex, INFINITE);
    RecordSyncData *data = GetRecordSyncData (newValue->num);

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
    ReleaseMutex (hRegistryMutex);
    return TRUE;
}
