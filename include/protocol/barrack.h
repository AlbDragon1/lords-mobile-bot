#ifndef _BARRACK_H_
#define _BARRACK_H_

#include <stdint.h>
#include <stdbool.h>

struct Connection;

typedef struct {
	bool loaded;
	uint32_t total;
	uint32_t infantry[4];
	uint32_t cavalry[4];
	uint32_t ranged[4];
	uint32_t siege[4];
	uint32_t t5_data[4];
} TroopData;

void RequestTroopTraining(struct Connection *c, uint8_t kind, uint8_t tier, uint32_t amount);
void CancelTroopTraining(struct Connection *c);

void RecvArmyGroupInfo(struct Connection*, const uint8_t*);

#endif