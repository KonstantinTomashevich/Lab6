#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <tchar.h>
#include "shared.h"

static HANDLE ConnectToServer ();
static DWORD MainCycle (HANDLE hPipe);
static void ReadCommand (HANDLE hPipe);
static void ModifyCommand (HANDLE hPipe);

int _tmain (int argc, char *argv[])
{
    HANDLE hPipe;
    hPipe = ConnectToServer ();
    if (hPipe == INVALID_HANDLE_VALUE)
    {
        return -1;
    }


    DWORD resultCode = MainCycle (hPipe);
    CloseHandle (hPipe);
    return resultCode;
}

static HANDLE ConnectToServer ()
{
    char pipeName [100];
    printf ("Input address prefix (like \\\\ip): ");
    scanf ("%s", pipeName);
    strcat (pipeName, "\\pipe\\OSLab6ServerNamedPipe");
    
    while (1)
    {
        HANDLE hPipe = CreateFile (pipeName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hPipe != INVALID_HANDLE_VALUE)
        {
            return hPipe;
        }

        if (GetLastError () != ERROR_PIPE_BUSY)
        {
            printf ("Could not open pipe. GLE=%ld\n", GetLastError ());
            return INVALID_HANDLE_VALUE;
        }


        if (!WaitNamedPipe (pipeName, 20000))
        {
            printf ("Could not open pipe: 20 second wait timed out.");
            return INVALID_HANDLE_VALUE;
        }
    }
}

static DWORD MainCycle (HANDLE hPipe)
{
    char lpvMessage[100];
    itoa (GetCurrentProcessId (), lpvMessage, 10);
    char chCommandBuffer[64];

    for (;;)
    {
        printf ("Input next command (read/modify/quit): ");
        scanf ("%63s", chCommandBuffer);

        if (strcmp (chCommandBuffer, "read") == 0)
        {
            ReadCommand (hPipe);
        }
        else if (strcmp (chCommandBuffer, "modify") == 0)
        {
            ModifyCommand (hPipe);
        }
        else if (strcmp (chCommandBuffer, "quit") == 0)
        {
            return 0;
        }
    }
}

static void ReadCommand (HANDLE hPipe)
{
    DWORD dMessageType;
    BOOL bLastResult;
    DWORD dResultCode;
    DWORD cbWritten;
    DWORD cbRead;

    DWORD dRecordCode;
    printf ("Input record code: ");
    scanf ("%ld", &dRecordCode);
    char chMessage[sizeof (DWORD) * 2];

    dMessageType = CC_READ;
    memcpy (chMessage, &dMessageType, sizeof (dMessageType));
    memcpy (chMessage + sizeof (dMessageType), &dRecordCode, sizeof (dRecordCode));

    if (!WriteFile (hPipe, chMessage, sizeof (DWORD) * 2, &cbWritten, NULL))
    {
        printf ("Unable to request read operation! GLE=%ld\n", GetLastError ());
        return;
    }

    do
    {
        bLastResult = ReadFile (hPipe, &dResultCode, sizeof (dMessageType), &cbRead, NULL);
    }
    while ((GetLastError () == ERROR_IO_PENDING || GetLastError () == ERROR_MORE_DATA) && !bLastResult);

    if (!bLastResult)
    {
        printf ("Unable to read reply! GLE=%ld\n", GetLastError ());
        return;
    }

    if (dResultCode)
    {
        TaxPayment tpResult;
        do
        {
            bLastResult = ReadFile (hPipe, &tpResult, sizeof (tpResult), &cbRead, NULL);
        }
        while (GetLastError () == ERROR_IO_PENDING && !bLastResult);

        if (!bLastResult)
        {
            printf ("Unable to read reply! GLE=%ld\n", GetLastError ());
            return;
        }

        printf ("ID: %d.\nName: %10s.\nSum: %lf.\n", tpResult.num, tpResult.name, tpResult.sum);
    }
    else
    {
        printf ("Read operation denied!\n");
    }
}

static void ModifyCommand (HANDLE hPipe)
{
    DWORD dMessageType;
    BOOL bLastResult;
    DWORD dResultCode;
    DWORD cbWritten;
    DWORD cbRead;

    DWORD dRecordCode;
    printf ("Input record code: ");
    scanf ("%ld", &dRecordCode);
    char chRequestMessage[sizeof (DWORD) * 2];

    dMessageType = CC_REQUEST_MODIFY;
    memcpy (chRequestMessage, &dMessageType, sizeof (dMessageType));
    memcpy (chRequestMessage + sizeof (dMessageType), &dRecordCode, sizeof (dRecordCode));

    if (!WriteFile (hPipe, &chRequestMessage, sizeof (DWORD) * 2, &cbWritten, NULL))
    {
        printf ("Unable to request modify operation! GLE=%ld\n", GetLastError ());
        return;
    }

    do
    {
        bLastResult = ReadFile (hPipe, &dResultCode, sizeof (dMessageType), &cbRead, NULL);
    }
    while (GetLastError () == ERROR_IO_PENDING && !bLastResult);

    if (!bLastResult)
    {
        printf ("Unable to read reply! GLE=%ld\n", GetLastError ());
        return;
    }

    if (dResultCode)
    {
        TaxPayment tpRecord;
        tpRecord.num = dRecordCode;

        printf ("Input new name: ");
        scanf ("%10s", tpRecord.name);

        printf ("Input new sum: ");
        scanf ("%lf", &tpRecord.sum);

        dMessageType = CC_MODIFY;
        char chModifyMessage[sizeof (DWORD) + sizeof (TaxPayment)];
        memcpy (chModifyMessage, &dMessageType, sizeof (dMessageType));
        memcpy (chModifyMessage + sizeof (dMessageType), &tpRecord, sizeof (tpRecord));

        if (!WriteFile (hPipe, &chModifyMessage, sizeof (DWORD) + sizeof (TaxPayment), &cbWritten, NULL))
        {
            printf ("Unable to send modify operation! GLE=%ld\n", GetLastError ());
            return;
        }

        do
        {
            bLastResult = ReadFile (hPipe, &dResultCode, sizeof (dResultCode), &cbRead, NULL);
        }
        while (GetLastError () == ERROR_IO_PENDING && !bLastResult);

        if (!bLastResult)
        {
            printf ("Unable to read reply! GLE=%ld\n", GetLastError ());
            return;
        }

        printf ("%s\n", dResultCode ? "Modified successfully!" : "Modify operation denied!");
    }
    else
    {
        printf ("Modify operation denied!\n");
    }
}
