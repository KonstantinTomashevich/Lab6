#pragma once
#include "shared.h"
#include <windows.h>

void InitServerDBCache (char *chServerDatabaseFileName);
void DestructServerDBCache ();

BOOL RequestModifyRecord (int id);
BOOL TryProcessReadCommand (int id, TaxPayment *output);
BOOL ProcessModifyCommand (TaxPayment *newValue);
