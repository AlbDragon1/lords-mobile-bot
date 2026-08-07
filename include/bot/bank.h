#ifndef _BANK_H_
#define _BANK_H_

#include <stdint.h>

#define BANK_MAGIC   0x4B4E4142u  /* "BANK" in little-endian */
#define BANK_VERSION 1u

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
	uint32_t capacity;
	BankRecord *record;
} BankData;

void LoadBank(struct Connection*);
void SaveBank(struct Connection*);

BankRecord *BankFindRecord(struct Connection *c, const char *name);
BankRecord *BankCreateRecord(struct Connection *c, const char *name);

#endif