#ifndef _BUILDING_H_
#define _BUILDING_H_

#include <stdint.h>
#include <stdbool.h>

struct Connection;

typedef enum
{
    BUILD_TIMBER        = 1,
    BUILD_STONE         = 2,
    BUILD_ORE           = 3,
    BUILD_FOOD          = 4,
    BUILD_MANOR         = 5,
    BUILD_BARRACKS      = 6,
    BUILD_INFIRMARY     = 7,
    BUILD_CASTLE        = 8,
    BUILD_VAULT         = 9,
    BUILD_ACADEMY       = 10,
    BUILD_WALL          = 12,
    BUILD_WATCHTOWER    = 13,
    BUILD_EMBASSY       = 14,
    BUILD_WORKSHOP      = 15,
    BUILD_TRADING_POST  = 17
} BuildingId;

static const uint32_t trading_post_supply_capacity[] = {
	0,      // Level 0 (unused)
	5000,   // Level 1
	15000,  // Level 2
	30000,  // Level 3
	50000,  // Level 4
	75000,  // Level 5
	105000,  // Level 6
	140000,  // Level 7
	180000,  // Level 8
	225000,  // Level 9
	275000,  // Level 10
	330000,  // Level 11
	400000,  // Level 12
	490000,  // Level 13
	600000,  // Level 14
	730000,  // Level 15
	880000,  // Level 16
	1050000,  // Level 17
	1250000,  // Level 18
	1450000,  // Level 19
	1650000,  // Level 20
	1850000,  // Level 21
	2050000,  // Level 22
	2250000,  // Level 23
	2500000,  // Level 24
	3000000,  // Level 25
};

bool IsBuilding(uint16_t build_id);
uint32_t GetTradingPostSupplyCapacity(uint8_t level);

const char *GetBuildingName(uint16_t build_id);

void RecvAllBuildData(struct Connection*, const uint8_t*);


#endif