#include "bot/command.h"
#include <ctype.h>
#include "items.h"
#include "utility.h"

#include "protocol/mail.h"


/*
 * NOTE:
 *
 * This module is currently a work in progress.
 * The implementation is functional but not considered final.
 * Additional optimizations, structural improvements, and new features
 * are expected as the project continues to evolve.
 */


void command_handler(Connection *c, const char *player_name, const char *message) {
	if (c->bot.command_prefix == 0) return;
	
	if (message[0] != c->bot.command_prefix) return;
	
	message++; // skip prefix 
	
	// handle food command 
	if (memcmp(message, "food", 4) == 0 && (message[4] == '\0' || message[4] == ' '))
	{
		ResourceCommandHandler(c, player_name, message + 4, RESOURCE_FOOD, "food");
		return;
	}
	
	// handle stone command
	if (memcmp(message, "stone", 5) == 0 && (message[5] == '\0' || message[5] == ' '))
	{
		ResourceCommandHandler(c, player_name, message + 5, RESOURCE_ROCK, "stone");
		return;
	}
	
	// handle wood command
	if (memcmp(message, "wood", 4) == 0 && (message[4] == '\0' || message[4] == ' '))
	{
		ResourceCommandHandler(c, player_name, message + 4, RESOURCE_WOOD, "wood");
		return;
	}
	
	// handle ore command
	if (memcmp(message, "ore", 3) == 0 && (message[3] == '\0' || message[3] == ' '))
	{
		ResourceCommandHandler(c, player_name, message + 3, RESOURCE_ORE, "ore");
		return;
	}
	
	// handle gold command
	if (memcmp(message, "gold", 4) == 0 && (message[4] == '\0' || message[4] == ' '))
	{
		ResourceCommandHandler(c, player_name, message + 4, RESOURCE_GOLD, "gold");
		return;
	}
	
	
}

static void ResourceCommandHandler(
    Connection *c,
    const char *player_name,
    const char *message,
    ResourceType type,
    const char *name
)
{
	
	if (c->transfer.state != TRANSFER_IDLE) {
		// Same player -> replace current pending request 
		if (strcmp(c->transfer.target_name, player_name) == 0) {
			memset(&c->transfer, 0, sizeof(c->transfer));
			c->transfer.state = TRANSFER_IDLE;
			// Continue below and assign the new request 
		} else {
			// Different player -> reject
			RequestSendMailFmt(
				c,
				player_name,
				"Transfer Busy",
				"Currently sending resources to %s. Use %cstop to cancel.",
				c->transfer.target_name,
				c->bot.command_prefix
			);
			
			return;
		}
	}
	
	char amount_str[32] = {0};
	
	if (sscanf(message, "%31s", amount_str) != 1)
		return;
	
	uint64_t amount = ParseNumber(amount_str);
	
	if (amount == 0 || amount > UINT32_MAX)
		return;
	
	uint32_t current = 0;
	uint32_t reserve = 0;
	
	current = c->resource.stock[type];
	reserve = c->bank.reserve[type];
	
	if (current <= reserve || amount > (current - reserve)) {
		char curr_amount_str[20];
		
		FormatNumber(current > reserve ? current - reserve : 0, curr_amount_str, sizeof(curr_amount_str));
		
		RequestSendMailFmt(
			c,
			player_name,
			"Not Enough Resources",
			"Only %s %s available.",
			curr_amount_str,
			name
		);
		return;
	}
	
	c->transfer.amount = (uint32_t)amount;
	c->transfer.remaining = (uint32_t)amount;
	c->transfer.resource_type = type;
	
	strcpy(c->transfer.target_name, player_name);
	
	c->transfer.state = TRANSFER_FIND_TARGET;
}
