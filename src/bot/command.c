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


static void ResourceCommandHandler(
    Connection *c,
    const char *player_name,
    const char *message,
    ResourceType type,
    const char *name
);

static void HelpCommandHandler(Connection *c, const char *player_name);

static bool IsAdmin(const Connection *c, const char *player_name)
{
	return c->bot.admin_name[0] != '\0' && strcmp(c->bot.admin_name, player_name) == 0;
}

/*
 * Match "<name>" or "<name> <args>" at the start of message.
 * Returns a pointer to the arguments (empty string if none), or NULL.
 */
static const char *MatchCommand(const char *message, const char *name)
{
	size_t len = strlen(name);
	
	if (strncmp(message, name, len) != 0)
		return NULL;
	
	if (message[len] == '\0')
		return message + len;
	
	if (message[len] == ' ')
		return message + len + 1;
	
	return NULL;
}

typedef struct {
	const char *name;
	ResourceType type;
} ResourceCommand;

static const ResourceCommand resource_commands[] = {
	{ "food",  RESOURCE_FOOD },
	{ "stone", RESOURCE_ROCK },
	{ "wood",  RESOURCE_WOOD },
	{ "ore",   RESOURCE_ORE  },
	{ "gold",  RESOURCE_GOLD },
};

void command_handler(Connection *c, const char *player_name, const char *message) {
	if (c->bot.command_prefix == 0) return;
	
	if (message[0] != c->bot.command_prefix) return;
	
	message++; // skip prefix 
	
	const char *args;
	
	// resource commands: food, stone, wood, ore, gold
	for (size_t i = 0; i < sizeof(resource_commands) / sizeof(resource_commands[0]); i++) {
		if ((args = MatchCommand(message, resource_commands[i].name)) != NULL) {
			ResourceCommandHandler(c, player_name, args, resource_commands[i].type, resource_commands[i].name);
			return;
		}
	}
	
	// handle balance command 
	if ((args = MatchCommand(message, "bal")) != NULL) {
		BalanceCommandHandler(c, player_name, args);
		return;
	}
	
	// Stop resources sending ($stop is an alias)
	if ((args = MatchCommand(message, "abort")) != NULL || (args = MatchCommand(message, "stop")) != NULL) {
		AbortCommandHandler(c, player_name, args);
		return;
	}
	
	// View resources sending 
	if ((args = MatchCommand(message, "status")) != NULL) {
		StatusCommandHandler(c, player_name, args);
		return;
	}
	
	if (MatchCommand(message, "help") != NULL) {
		HelpCommandHandler(c, player_name);
		return;
	}
}

static void HelpCommandHandler(Connection *c, const char *player_name)
{
	char p = c->bot.command_prefix;
	
	RequestSendMailFmt(
		c,
		player_name,
		"Bot Commands",
		"%cfood <amount> [target], %cstone, %cwood, %core, %cgold\n"
		"  Request resources, e.g. %cfood 10M\n"
		"%cabort - cancel the current transfer (alias: %cstop)\n"
		"%cstatus - show the current transfer\n"
		"%cbal - show bank balance (admin)\n"
		"%chelp - show this message",
		p, p, p, p, p, p, p, p, p, p, p
	);
}

static void ResourceCommandHandler(
    Connection *c,
    const char *player_name,
    const char *message,
    ResourceType type,
    const char *name
)
{
	bool admin = IsAdmin(c, player_name);
	
	// Bank closed: banking commands are ignored (admin may still use them)
	if (!c->bank.enabled && !admin) return;
	
	if (!c->bank.allowed[type] && !admin) {
		RequestSendMailFmt(
			c,
			player_name,
			"Transfer Unavailable",
			"%s transfers are disabled by the bank configuration.",
			name
		);
		
		return;
	}
	
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
				"Currently sending resources to %s. Use %cabort to cancel.",
				c->transfer.target_name,
				c->bot.command_prefix
			);
			
			return;
		}
	}
	
	char amount_str[32] = {0};
	char target_name[13] = {0};
	
	int n = sscanf(message, "%31s %12[^\n]", amount_str, target_name);
	
	uint64_t amount = (n >= 1) ? ParseNumber(amount_str) : 0;
	
	if (amount == 0 || amount > UINT32_MAX) {
		RequestSendMailFmt(c, player_name, "Usage", "Usage: %c%s <amount> [target name], e.g. %c%s 10M",
			c->bot.command_prefix, name, c->bot.command_prefix, name);
		return;
	}
	
	uint32_t current = c->resource.stock[type];
	uint32_t reserve = c->bank.reserve[type];
	
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
	
	snprintf(c->transfer.issued_name, sizeof(c->transfer.issued_name), "%s", player_name);
	snprintf(c->transfer.target_name, sizeof(c->transfer.target_name), "%s",
		n == 2 ? target_name : player_name);
	
	c->transfer.state = TRANSFER_FIND_TARGET;
}

void BalanceCommandHandler(Connection *c, const char *player_name, const char *message) {
	// If no authorized name is configured, everyone is allowed to view
	// the balance and perform bank-related commands.
	if (c->bot.admin_name[0] != '\0') {
		if (strcmp(c->bot.admin_name, player_name) != 0) return;
	}
	
	// temporary hold buffer 
	char bank_food[20];
	char bank_rock[20];
	char bank_wood[20];
	char bank_ore [20];
	char bank_gold[20];
	
	// Convert number to human readable format e.g 100k, 1M, 1B
	FormatNumber(c->resource.stock[0], bank_food, sizeof(bank_food));
	FormatNumber(c->resource.stock[1], bank_rock, sizeof(bank_rock));
	FormatNumber(c->resource.stock[2], bank_wood, sizeof(bank_wood));
	FormatNumber(c->resource.stock[3], bank_ore,  sizeof(bank_ore));
	FormatNumber(c->resource.stock[4], bank_gold, sizeof(bank_gold));
	
	// Send back result to command issuer
	RequestSendMailFmt(c, player_name, "Bank Balance", 
		"Food: %s | Stone: %s | Wood: %s | Ore: %s | Gold: %s\n",
		bank_food, bank_rock, bank_wood, bank_ore, bank_gold
	);
	return;
}


void AbortCommandHandler(Connection *c, const char *player_name, const char *message)
{
	(void)message;
	
	if (c->transfer.state == TRANSFER_IDLE) {
		RequestSendMail(c, player_name, "Transfer", "No resource transfer is currently in progress.");
		return;
	}
	
	// A resource transfer is currently in progress.
	{
		// The configured administrator may cancel any active transfer.
		if (IsAdmin(c, player_name)) {
			RequestSendMailFmt(
				c,
				player_name,
				"Transfer Aborted",
				"Resource transfer to %s has been cancelled by %s.",
				c->transfer.target_name,
				player_name
			);
			
			memset(&c->transfer, 0, sizeof(c->transfer));
			c->transfer.state = TRANSFER_IDLE;
			return;
		}
		
		// Regular players may only cancel transfers they requested or receive.
		if (strcmp(c->transfer.target_name, player_name) == 0 ||
		    strcmp(c->transfer.issued_name, player_name) == 0) {
			memset(&c->transfer, 0, sizeof(c->transfer));
			c->transfer.state = TRANSFER_IDLE;
			RequestSendMailFmt(
				c,
				player_name,
				"Transfer Aborted",
				"Your resource transfer has been cancelled."
			);
			return;
		}
		
		// Another player's transfer is in progress.
		RequestSendMailFmt(
			c,
			player_name,
			"Action Not Allowed",
			"A resource transfer for %s is currently in progress. "
			"Only that player or the administrator can cancel it.",
			c->transfer.target_name
		);
		return;
	}
}

static const char *ResourceTypeName(uint8_t type) {
	switch (type) {
		case 0: 
			return "food";
		case 1:
			return "stone";
		case 2:
			return "wood";
		case 3: 
			return "ore";
		case 4: 
			return "gold";
		default: 
			return "unknown";
	}
}

void StatusCommandHandler(Connection *c, const char *player_name, const char *message)
{
	if (c->transfer.state == TRANSFER_IDLE) {
		RequestSendMail(
			c,
			player_name,
			"Transfer Status",
			"No resource transfer is currently in progress."
		);
		return;
	}
	
	char amount[20];
	char remaining[20];
	
	FormatNumber(c->transfer.amount, amount, sizeof(amount));
	FormatNumber(c->transfer.remaining, remaining, sizeof(remaining));
	
	RequestSendMailFmt(
		c,
		player_name,
		"Transfer Status",
		"Target: %s\n"
		"Resource: %s\n"
		"Total: %s\n"
		"Remaining: %s\n",
		// "State: %u",
		c->transfer.target_name,
		ResourceTypeName(c->transfer.resource_type),
		amount,
		remaining
		// c->transfer.state
	);
}