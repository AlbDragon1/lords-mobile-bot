#include "bot/tick.h"
#include "connection.h"
#include <time.h>

#include "utility.h"

#include "log.h"

#include "bot/transfer.h" // resources transfer 

void BotTick(Connection *c)
{
	if (c->server_time == 0) return;
	
	ResourceTransferTick(c); // Important line
	
	time_t now = time(NULL);
	
	if (now - c->last_tick_time < 15) return;
	
	c->last_tick_time = now;
	
	RequestHeartBeat(c);
	
	// restart scheduler
	if (c->action_state >= ACTION_MAX) {
		c->action_state = 1;
	}
	
	switch (c->action_state) {
		case ACTION_CARGO: 
			CargoShipTick(c);
			LOGD("Called CargoShipTick()\n");
			break;
		case ACTION_SHIELD:
			// ShieldTick(c);
			LOGD("Called ShieldTick()\n");
			break;
		case ACTION_HEAL:
			// HealTick(c);
			LOGD("Called HealTick()\n");
			break;
		case ACTION_GATHER:
			// GatherTick(c);
			LOGD("Called GatherTick()\n");
			break;
		default: 
			break;
	}
	
	c->action_state++;
}