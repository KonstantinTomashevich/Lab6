#include "server_db_cache.h"
#include <stdio.h>

typedef struct
{
    unsigned int readersCount;
    DWORD writer;
} RecordSyncData;

#define NO_WRITER -1
#define DB_RECORDS_COUNT 1000

char *chServerDatabase;
RecordSyncData recordSyncData[DB_RECORDS_COUNT] = {0};
HANDLE hRegistryMutex;

void InitServerDBCache (char *chServerDatabaseFileName)
{
    chServerDatabase = chServerDatabaseFileName;
    for (int index = 0; index < DB_RECORDS_COUNT; ++index)
    {
        recordSyncData[index].readersCount = 0;
        recordSyncData[index].writer = NO_WRITER;
    }

    hRegistryMutex = CreateMutex (NULL, FALSE, NULL);
}

BOOL RequestModifyRecord (int id)
{
    WaitForSingleObject (hRegistryMutex, INFINITE);
    if (recordSyncData[id].readersCount > 0 || recordSyncData[id].writer != NO_WRITER)
    {
        ReleaseMutex (hRegistryMutex);
        return FALSE;
    }

    recordSyncData[id].writer = GetCurrentThreadId ();
    ReleaseMutex (hRegistryMutex);
    return TRUE;
}

BOOL TryProcessReadCommand (int id, TaxPayment *output)
{
    WaitForSingleObject (hRegistryMutex, INFINITE);
    if (recordSyncData[id].writer != NO_WRITER)
    {
        ReleaseMutex (hRegistryMutex);
        return FALSE;
    }

    recordSyncData[id].readersCount++;
    ReleaseMutex (hRegistryMutex);

    FILE *db = fopen (chServerDatabase, "rb");
    fseek (db, id * sizeof (TaxPayment), SEEK_SET);
    fread (output, sizeof (TaxPayment), 1, db);
    fclose (db);

    WaitForSingleObject (hRegistryMutex, INFINITE);
    recordSyncData[id].readersCount--;
    ReleaseMutex (hRegistryMutex);
    return TRUE;
}

BOOL ProcessModifyCommand (TaxPayment *newValue)
{
    WaitForSingleObject (hRegistryMutex, INFINITE);
    if (recordSyncData[newValue->num].writer != GetCurrentThreadId ())
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
    recordSyncData[newValue->num].writer = NO_WRITER;
    ReleaseMutex (hRegistryMutex);
    return TRUE;
}
