#pragma once
#define BUFFER_SIZE 512

typedef enum
{
    CC_REQUEST_MODIFY = 0,
    CC_READ,
    CC_MODIFY,
    COMMANDS_COUNT
} CommandCodes;

typedef struct
{
    int num;
    char name[10];
    double sum;
} TaxPayment;

