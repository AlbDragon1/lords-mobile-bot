#include "bot/transfer.h"
#include "protocol/resource.h"
#include "protocol/alliance.h"

#include "protocol/mail.h"

#include "connection.h"

#include <time.h>

uint32_t CalculateTransferAmount(Connection *c)
{
	uint8_t type = c->transfer.resource_type;
	uint32_t current = c->resource.stock[type];
	uint32_t reserve = c->bank.reserve[type];
	
	/* Keep reserved resources. */
	if (current <= reserve) 
		return 0;
	
	uint32_t available = current - reserve;
	
	/* Don't send more than requested. */
	if (available > c->transfer.remaining)
		available = c->transfer.remaining;
	
	/* Don't exceed Trading Post capacity. */
	if (available > c->supply_capacity)
		available = c->supply_capacity;
	
	return available;
}

void SendResourceMarch(Connection *c) {
	if (c->player.current_marches >= c->player.max_marches) {
		c->transfer.state = TRANSFER_WAIT_MARCH;
		return;
	}
	
	if (c->transfer.remaining == 0) {
		c->transfer.state = TRANSFER_COMPLETE;
		return;
	}
	
	uint32_t amount = CalculateTransferAmount(c);
	
	if (amount == 0) {
		c->transfer.state = TRANSFER_COMPLETE;
		return;
	}
	
	uint8_t type = c->transfer.resource_type;
	
	uint32_t stock[5] = {0};
	
	stock[type] = amount;
	
	SendResource(c, stock, c->transfer.zone_id, c->transfer.point_id);
	
	c->transfer.remaining -= amount;
	c->transfer.state = TRANSFER_WAIT_MARCH;
	
	return;
}


void ResourceTransferTick(Connection *c)
{
	if (c->transfer.state == TRANSFER_IDLE) return;
	
	// Bot doesn't have trading post yet
	if (c->supply_capacity == 0) return;
	
	switch (c->transfer.state) {
		case TRANSFER_FIND_TARGET:
			/* Find player's location */
			RequestAllyPoint(c, c->transfer.target_name);
			c->transfer.timeout = time(NULL) + 10;   // wait up to 10 seconds
			c->transfer.state = TRANSFER_WAIT_TARGET;
			// printf("called TRANSFER_FIND_TARGET\n");
			break;
		case TRANSFER_WAIT_TARGET: 
			if (time(NULL) >= c->transfer.timeout) {
				c->transfer.state = TRANSFER_FAILED;
				// printf("called TRANSFER_WAIT_TARGET lookup timed out\n");
			}
			break;
		case TRANSFER_SEND_MARCH:
			SendResourceMarch(c);
			// printf("called TRANSFER_SEND_MARCH\n");
			// printf("[INFO ] max march: %u, cur march: %u\n", c->player.max_marches, c->player.current_marches);
			break;
		case TRANSFER_WAIT_MARCH:
			/* Wait until march returns */
			
			break;
		case TRANSFER_COMPLETE:
			c->transfer.state = TRANSFER_IDLE;
			break;
		case TRANSFER_FAILED:
			c->transfer.state = TRANSFER_IDLE;
			break;
		case TRANSFER_FAILED_TOO_FAR:
			RequestSendMailFmt(
				c,
				c->transfer.issued_name,
				"Transfer Failed",
				"Target is too far away. Maximum delivery distance is %u tiles.",
				c->bank.max_delivery_distance
			);
			c->transfer.state = TRANSFER_IDLE;
			break;
		default:
			break;
	}
}

