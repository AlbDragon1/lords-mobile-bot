#ifndef _BANK_H_
#define _BANK_H_

#include <stdint.h>

struct Connection;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t count;
} BankHeader;

typedef struct {
	char name[13];
	uint32_t stock[5];
} BankRecord;

typedef struct {
	BankHeader header;
	BankRecord *record;
} BankData;

void LoadBank(struct Connection*);
void SaveBank(struct Connection*);

#endif