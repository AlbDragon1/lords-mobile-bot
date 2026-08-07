#include "bot/tick.h"
#include "connection.h"
#include <time.h>

#include "utility.h"

#include "log.h"

#include "bot/transfer.h" // resources transfer 

#include "net_rw.h"
#include "packet_enum.h"

#include "protocol/barrack.h"

void ServerMagicGateDoEvent(Connection *c, uint16_t n, uint8_t x) {
	c->size = 2;
	
	// packet type 
    write_u16(c->data + c->size, _MSG_REQUEST_MAGIC_GATE_DO_EVENT);
    c->size += 2;
    
    // Sequence 
    write_u32(c->data + c->size, ++c->protocol.seq_id);
    c->size += 4;
    
    // 
    write_u16(c->data + c->size, n);
    c->size += 2;
    
    write_u8(c->data + c->size, x);
    c->size += 1;
    
    write_u16(c->data, c->size);

    send_packet(c, true);
}


void RequestTD(Connection *c) {
	c->size = 2;
	
	// packet type 
    write_u16(c->data + c->size, _MSG_REQUEST_TD_TRIGGER);
    c->size += 2;
    
    // Sequence 
    write_u32(c->data + c->size, ++c->protocol.seq_id);
    c->size += 4;
    
    // 
    write_u8(c->data + c->size, 0);
    c->size += 1;
    
    write_u16(c->data, c->size);

    send_packet(c, true);
}



static uint16_t n = 0;

void BotTick(Connection *c)
{
	if (c->server_time == 0) return;
	
	ResourceTransferTick(c); // Important line
	
	// ServerMagicGateDoEvent(c, n++, 0);
	//printf("ServerMagicGateDoEvent(c, %u, 0);\n", n);
	
	// RequestTD(c);
	// printf("RequestTD(c);\n");
	
	time_t now = time(NULL);
	
	if (now - c->last_tick_time < 5) return;
	
	c->last_tick_time = now;
	
	RequestHeartBeat(c);
	
	// restart scheduler
	if (c->action_state >= ACTION_MAX) {
		c->action_state = 1;
	}
	
	switch (c->action_state) {
		case ACTION_CARGO: 
			CargoShipTick(c);
			// LOGI("Called CargoShipTick()\n");
			break;
		case ACTION_SHIELD:
			// ShieldTick(c);
			// LOGI("Called ShieldTick()\n");
			break;
		case ACTION_HEAL:
			// HealTick(c);
			// LOGI("Called HealTick()\n");
			// RequestTroopTraining(c, 0, 0, 1000);
			break;
		case ACTION_GATHER:
			// GatherTick(c);
			// LOGI("Called GatherTick()\n");
			break;
		default: 
			break;
	}
	
	c->action_state++;
}