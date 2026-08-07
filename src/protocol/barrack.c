#include "protocol/barrack.h"
#include "net_rw.h"
#include "packet_enum.h"
#include "connection.h"
#include "log.h"

void RequestTroopTraining(Connection *c, uint8_t kind, uint8_t tier, uint32_t amount) {
	c->size = 2; // reserve space for packet length
	
	// write packet type 
	write_u16(c->data + c->size, _MSG_REQUEST_TRAINING_); c->size += 2;
	write_u32(c->data + c->size, ++c->protocol.seq_id);   c->size += 4;
	write_u8 (c->data + c->size, kind);                   c->size += 1;// write RD_Kind troop type (infantry, ranged, cavalry, siege)
	write_u8 (c->data + c->size, tier);                   c->size += 1;// write RD_Rank (server expects 0-based) (t1, t2, t3, t4, t5)
	write_u32(c->data + c->size, amount);                 c->size += 4;// write troop amount 
	write_u16(c->data, c->size); // update packet size
	
	send_packet(c, true);
}

void CancelTroopTraining(Connection *c)
{
	c->size = 2;
	
	write_u16(c->data + c->size, _MSG_REQUEST_CANCELTRAINING); c->size += 2;
	write_u32(c->data + c->size, ++c->protocol.seq_id);        c->size += 4;
	write_u16(c->data, c->size); // update packet size
	
	send_packet(c, true);
}


void RecvArmyGroupInfoLog(Connection *c) {
	printf("Total Troop: %u\n", c->troop.total);
	
	
	for (int i = 3; i >= 0; i--) {
		printf("T%d Infantry: %u\n", i + 1, c->troop.infantry[i]);
	}
	
	for (int i = 3; i >= 0; i--) {
		printf("T%d Ranged: %u\n", i + 1, c->troop.ranged[i]);
	}
	
	for (int i = 3; i >= 0; i--) {
		printf("T%d Cavalry: %u\n", i + 1, c->troop.cavalry[i]);
	}
	
	for (int i = 3; i >= 0; i--) {
		printf("T%d Siege: %u\n", i + 1, c->troop.siege[i]);
	}
	
}

void RecvArmyGroupInfo(Connection *c, const uint8_t *data) {
	uint16_t offset = 0;
	
	for (int index = 0; index < 4; index++) {
		c->troop.infantry[index] = read_u32(data + offset); offset += 4;
		c->troop.total += c->troop.infantry[index];
	}
	
	for (int index = 0; index < 4; index++) {
		c->troop.ranged[index] = read_u32(data + offset); offset += 4;
		c->troop.total += c->troop.ranged[index];
	}
	
	for (int index = 0; index < 4; index++) {
		c->troop.cavalry[index] = read_u32(data + offset); offset += 4;
		c->troop.total += c->troop.cavalry[index];
	}
	
	for (int index = 0; index < 4; index++) {
		c->troop.siege[index] = read_u32(data + offset); offset += 4;
		c->troop.total += c->troop.siege[index];
	}
	
	c->troop.loaded = true;
	
	// RecvArmyGroupInfoLog(c);
	return;
}



