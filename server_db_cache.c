#include "server_db_cache.h"
#include <stdio.h>

typedef struct
{
    unsigned int readersCount;
    DWORD writter;
} RecordSyncData;

#define NO_WRITTER -1

char *chServerDatabase;
RecordSyncData recordSyncData[1000] = {0};
TaxPayment payments[1000] = {0};
HANDLE hRegistryMutex;

void InitServerDBCache (char *chServerDatabaseFileName)
{
    chServerDatabase = chServerDatabaseFileName;
    FILE *dbFile = fopen (chServerDatabaseFileName, "rb");
    for (int index = 0; index < 1000; ++index)
    {
        fread (payments + index, sizeof (TaxPayment), 1, dbFile);
        recordSyncData[index].readersCount = 0;
        recordSyncData[index].writter = NO_WRITTER;
    }

    fclose (dbFile);
    hRegistryMutex = CreateMutex (NULL, FALSE, NULL);
}

BOOL RequestModifyRecord (int id)
{
    WaitForSingleObject (hRegistryMutex, INFINITE);
    if (recordSyncData[id].readersCount > 0 || recordSyncData[id].writter != NO_WRITTER)
    {
        ReleaseMutex (hRegistryMutex);
        return FALSE;
    }

    recordSyncData[id].writter = GetCurrentThreadId ();
    ReleaseMutex (hRegistryMutex);
    return TRUE;
}

BOOL TryProcessReadCommand (int id, TaxPayment *output)
{
    WaitForSingleObject (hRegistryMutex, INFINITE);
    if (recordSyncData[id].writter != NO_WRITTER)
    {
        ReleaseMutex (hRegistryMutex);
        return FALSE;
    }

    recordSyncData[id].readersCount++;
    ReleaseMutex (hRegistryMutex);

    memcpy (output, payments + id, sizeof (TaxPayment));
    WaitForSingleObject (hRegistryMutex, INFINITE);
    recordSyncData[id].readersCount--;
    ReleaseMutex (hRegistryMutex);
    return TRUE;
}

BOOL ProcessModifyCommand (TaxPayment *newValue)
{
    WaitForSingleObject (hRegistryMutex, INFINITE);
    if (recordSyncData[newValue->num].writter != GetCurrentThreadId ())
    {
        ReleaseMutex (hRegistryMutex);
        return FALSE;
    }

    ReleaseMutex (hRegistryMutex);
    memcpy (payments + newValue->num, newValue, sizeof (TaxPayment));

    WaitForSingleObject (hRegistryMutex, INFINITE);
    recordSyncData[newValue->num].writter = NO_WRITTER;
    ReleaseMutex (hRegistryMutex);
    return TRUE;
}
