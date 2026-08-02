#include "protocol/building.h"
#include "net_rw.h"
#include "packet_enum.h"
#include "connection.h"
#include "log.h"

bool IsBuilding(uint16_t build_id)
{
    switch (build_id)
    {
        case 1:   // Timber
        case 2:   // Stone
        case 3:   // Ore
        case 4:   // Food
        case 5:   // Manor
        case 6:   // Barracks
        case 7:   // Infirmary
        case 8:   // Castle
        case 9:   // Vault
        case 10:  // Academy
        case 12:  // Wall
        case 13:  // Watchtower
        case 14:  // Embassy
        case 15:  // Workshop
        case 17:  // Trading Post
            return true;

        default:
            return false;
    }
}

const char *GetBuildingName(uint16_t build_id)
{
    switch (build_id)
    {
        case 1:  return "Timber";
        case 2:  return "Stone";
        case 3:  return "Ore";
        case 4:  return "Food";
        case 5:  return "Manor";
        case 6:  return "Barracks";
        case 7:  return "Infirmary";
        case 8:  return "Castle";
        case 9:  return "Vault";
        case 10: return "Academy";
        case 12: return "Wall";
        case 13: return "Watchtower";
        case 14: return "Embassy";
        case 15: return "Workshop";
        case 17: return "Trading Post";
        default: return "Unknown";
    }
}

uint32_t GetTradingPostSupplyCapacity(uint8_t level) {
	uint8_t lv = level > 25 ? 25 : level;
	return trading_post_supply_capacity[lv];
}

void RecvAllBuildData(Connection *c, const uint8_t *data)
{
	uint16_t offset = 0;
	
	c->building_count = read_u8(data + offset); offset += 1;
	
	// printf("building:\n");
	uint8_t trading_post_lv = 0;
	
	for (int i = 0; i < c->building_count; i++) {
		c->building[i].position_id = read_u16(data + offset); offset += 2;
		c->building[i].build_id    = read_u16(data + offset); offset += 2;
		c->building[i].level       = read_u8(data + offset);  offset += 1;
		
		if (c->building[i].build_id == BUILD_TRADING_POST) {
			trading_post_lv = c->building[i].level;
		}
		
		// display only building 
		if (IsBuilding(c->building[i].build_id) == false) continue;
		
		// filter 
		// if (c->building[i].build_id != BUILD_MANOR) continue;
		
		/*
		printf("Building Name: %s\n", GetBuildingName(c->building[i].build_id));
		printf("Building Pos: %u\n", c->building[i].position_id);
		printf("Building Level: %u\n", c->building[i].level);
		
		printf("\n");
		*/
		
	}
	
	c->supply_capacity += GetTradingPostSupplyCapacity(trading_post_lv);
	
}
