#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <stdlib.h>

#include "shared.h"
#include "server_db_cache.h"

static void CreateNewDatabase ();
static DWORD WINAPI InstanceThread (LPVOID);

static void GetAnswerToRequest (CommandCodes command, HANDLE hPipe);
static void GetAnswerToModifyRequestCommand (HANDLE hPipe);
static void GetAnswerToReadCommand (HANDLE hPipe);
static void GetAnswerToModifyCommand (HANDLE hPipe);

#define DB_RECORDS_COUNT 1000
char chServerDatabaseFileName[MAX_PATH];
HANDLE hConsoleMutex;

int _tmain (VOID)
{
    printf ("Input database file name to create: ");
    scanf ("%s", chServerDatabaseFileName);
    CreateNewDatabase ();
    InitServerDBCache (chServerDatabaseFileName);
    hConsoleMutex = CreateMutex (NULL, FALSE, NULL);

    BOOL fConnected;
    DWORD dwThreadId = 0;
    HANDLE hPipe;
    HANDLE hThread = NULL;
    LPTSTR lpszPipename = "\\\\.\\pipe\\OSLab6ServerNamedPipe";

    for (;;)
    {
        WaitForSingleObject (hConsoleMutex, INFINITE);
        printf ("\nPipe Server: Main thread awaiting client connection on %s\n", lpszPipename);
        ReleaseMutex (hConsoleMutex);

        hPipe = CreateNamedPipe (lpszPipename, PIPE_ACCESS_DUPLEX,
                                 (PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_ACCEPT_REMOTE_CLIENTS) &
                                 ~PIPE_REJECT_REMOTE_CLIENTS,
                                 PIPE_UNLIMITED_INSTANCES, BUFFER_SIZE, BUFFER_SIZE, 0, NULL);

        if (hPipe == INVALID_HANDLE_VALUE)
        {
            WaitForSingleObject (hConsoleMutex, INFINITE);
            printf ("CreateNamedPipe failed, GLE=%ld.\n", GetLastError ());
            ReleaseMutex (hConsoleMutex);
            return -1;
        }

        fConnected = ConnectNamedPipe (hPipe, NULL) ? TRUE : (GetLastError () == ERROR_PIPE_CONNECTED);
        if (fConnected)
        {
            WaitForSingleObject (hConsoleMutex, INFINITE);
            printf ("Client connected, creating a processing thread.\n");
            ReleaseMutex (hConsoleMutex);
            hThread = CreateThread (NULL, 0, InstanceThread, (LPVOID) hPipe, 0, &dwThreadId);

            if (hThread == NULL)
            {
                WaitForSingleObject (hConsoleMutex, INFINITE);
                printf ("CreateThread failed, GLE=%ld.\n", GetLastError ());
                ReleaseMutex (hConsoleMutex);
                return -1;
            }
            else
            {
                CloseHandle (hThread);
            }
        }
        else
        {
            CloseHandle (hPipe);
        }
    }

    CloseHandle (hConsoleMutex);
    DestructServerDBCache ();
    return 0;
}

static void CreateNewDatabase ()
{
    FILE *dbFile = fopen (chServerDatabaseFileName, "wb");
    TaxPayment tpRecord;
    strcpy (tpRecord.name, "#");

    for (int index = 0; index < DB_RECORDS_COUNT; ++index)
    {
        tpRecord.num = index;
        itoa (index, tpRecord.name + 1, 10);
        tpRecord.sum = index * 1.1;
        fwrite (&tpRecord, sizeof (tpRecord), 1, dbFile);
    }

    fclose (dbFile);
}

static DWORD WINAPI InstanceThread (LPVOID lpvParam)
{
    DWORD cbBytesRead = 0;
    BOOL fSuccess;
    HANDLE hPipe = NULL;
    DWORD commandCode;

    if (lpvParam == NULL)
    {
        WaitForSingleObject (hConsoleMutex, INFINITE);
        printf ("\nERROR - Pipe Server Failure:\n");
        printf ("   InstanceThread got an unexpected NULL value in lpvParam.\n");
        printf ("   InstanceThread exitting.\n");
        ReleaseMutex (hConsoleMutex);
        return (DWORD) -1;
    }

    WaitForSingleObject (hConsoleMutex, INFINITE);
    printf ("InstanceThread %ld created, receiving and processing messages.\n", GetCurrentThreadId ());
    ReleaseMutex (hConsoleMutex);
    hPipe = (HANDLE) lpvParam;

    while (1)
    {
        fSuccess = ReadFile (hPipe, &commandCode, sizeof (DWORD), &cbBytesRead, NULL);
        if ((!fSuccess || cbBytesRead == 0) &&
        !(GetLastError () == ERROR_IO_PENDING || GetLastError () == ERROR_MORE_DATA))
        {
            if (GetLastError () == ERROR_BROKEN_PIPE)
            {
                WaitForSingleObject (hConsoleMutex, INFINITE);
                printf ("InstanceThread %ld: client disconnected, error: %ld.\n",
                    GetCurrentThreadId (), GetLastError ());
                ReleaseMutex (hConsoleMutex);
            }
            else
            {
                WaitForSingleObject (hConsoleMutex, INFINITE);
                printf ("InstanceThread %ld ReadFile failed, GLE=%ld.\n", GetCurrentThreadId (), GetLastError ());
                ReleaseMutex (hConsoleMutex);
            }

            break;
        }


        GetAnswerToRequest ((CommandCodes) commandCode, hPipe);
    }

    FlushFileBuffers (hPipe);
    DisconnectNamedPipe (hPipe);
    CloseHandle (hPipe);

    WaitForSingleObject (hConsoleMutex, INFINITE);
    printf ("InstanceThread %ld exitting.\n", GetCurrentThreadId ());
    ReleaseMutex (hConsoleMutex);
    return 1;
}

static void GetAnswerToRequest (CommandCodes command, HANDLE hPipe)
{
    if (command == CC_REQUEST_MODIFY)
    {
        GetAnswerToModifyRequestCommand (hPipe);
    }
    else if (command == CC_READ)
    {
        GetAnswerToReadCommand (hPipe);
    }
    else if (command == CC_MODIFY)
    {
        GetAnswerToModifyCommand (hPipe);
    }
}

static void GetAnswerToModifyRequestCommand (HANDLE hPipe)
{
    WaitForSingleObject (hConsoleMutex, INFINITE);
    printf ("GetAnswerToModifyRequestCommand %ld: message received.\n", GetCurrentThreadId ());
    ReleaseMutex (hConsoleMutex);

    DWORD dRecordCode;
    DWORD cbRead;
    DWORD cbWritten;
    BOOL bLastResult;

    do
    {
        bLastResult = ReadFile (hPipe, &dRecordCode, sizeof (dRecordCode), &cbRead, NULL);
    }
    while (GetLastError () == ERROR_IO_PENDING && !bLastResult);

    if (!bLastResult)
    {
        WaitForSingleObject (hConsoleMutex, INFINITE);
        printf ("GetAnswerToModifyRequestCommand %ld: Unable to read command data! GLE=%ld\n",
            GetCurrentThreadId (), GetLastError ());
        ReleaseMutex (hConsoleMutex);
        return;
    }

    DWORD answer = RequestModifyRecord (dRecordCode) ? 1 : 0;
    if (!WriteFile (hPipe, &answer, sizeof (DWORD), &cbWritten, NULL))
    {
        WaitForSingleObject (hConsoleMutex, INFINITE);
        printf ("GetAnswerToModifyRequestCommand %ld: Unable to write answer to request modify operation! GLE=%ld\n",
            GetCurrentThreadId (), GetLastError ());
        ReleaseMutex (hConsoleMutex);
        return;
    }
}

static void GetAnswerToReadCommand (HANDLE hPipe)
{
    WaitForSingleObject (hConsoleMutex, INFINITE);
    printf ("GetAnswerToReadCommand %ld: message received.\n", GetCurrentThreadId ());
    ReleaseMutex (hConsoleMutex);

    DWORD dRecordCode;
    DWORD cbRead;
    DWORD cbWritten;
    BOOL bLastResult;

    do
    {
        bLastResult = ReadFile (hPipe, &dRecordCode, sizeof (dRecordCode), &cbRead, NULL);
    }
    while (GetLastError () == ERROR_IO_PENDING && !bLastResult);

    if (!bLastResult)
    {
        WaitForSingleObject (hConsoleMutex, INFINITE);
        printf ("GetAnswerToReadCommand %ld: Unable to read command data! GLE=%ld\n",
            GetCurrentThreadId (), GetLastError ());
        ReleaseMutex (hConsoleMutex);
        return;
    }

    TaxPayment tpResult;
    DWORD answer = TryProcessReadCommand (dRecordCode, &tpResult);

    if (!WriteFile (hPipe, &answer, sizeof (DWORD), &cbWritten, NULL))
    {
        WaitForSingleObject (hConsoleMutex, INFINITE);
        printf ("GetAnswerToReadCommand %ld: Unable to write answer to read operation! GLE=%ld\n",
            GetCurrentThreadId (), GetLastError ());
        ReleaseMutex (hConsoleMutex);
        return;
    }

    if (answer && !WriteFile (hPipe, &tpResult, sizeof (tpResult), &cbWritten, NULL))
    {
        WaitForSingleObject (hConsoleMutex, INFINITE);
        printf ("GetAnswerToReadCommand %ld: Unable to write record for read command! GLE=%ld\n",
                GetCurrentThreadId (), GetLastError ());
        ReleaseMutex (hConsoleMutex);
        return;
    }
}

static void GetAnswerToModifyCommand (HANDLE hPipe)
{
    WaitForSingleObject (hConsoleMutex, INFINITE);
    printf ("GetAnswerToModifyCommand %ld: message received.\n", GetCurrentThreadId ());
    ReleaseMutex (hConsoleMutex);

    TaxPayment tpNewValue;
    DWORD cbRead;
    DWORD cbWritten;
    BOOL bLastResult;

    do
    {
        bLastResult = ReadFile (hPipe, &tpNewValue, sizeof (tpNewValue), &cbRead, NULL);
    }
    while (GetLastError () == ERROR_IO_PENDING && !bLastResult);

    if (!bLastResult)
    {
        WaitForSingleObject (hConsoleMutex, INFINITE);
        printf ("GetAnswerToModifyCommand %ld: Unable to read command data! GLE=%ld\n",
            GetCurrentThreadId (), GetLastError ());
        ReleaseMutex (hConsoleMutex);
        return;
    }

    DWORD answer = ProcessModifyCommand (&tpNewValue);
    if (!WriteFile (hPipe, &answer, sizeof (DWORD), &cbWritten, NULL))
    {
        WaitForSingleObject (hConsoleMutex, INFINITE);
        printf ("GetAnswerToModifyCommand %ld: Unable to write answer to modify operation! GLE=%ld\n",
            GetCurrentThreadId (), GetLastError ());
        ReleaseMutex (hConsoleMutex);
        return;
    }
}
