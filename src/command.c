#include "command.h"
#include <ctype.h>
#include "items.h"
#include "protocol.h"
#include "config.h"

/*
 * NOTE:
 *
 * This module is currently a work in progress.
 * The implementation is functional but not considered final.
 * Additional optimizations, structural improvements, and new features
 * are expected as the project continues to evolve.
 */
 
/*
void format_number2(uint64_t num, char *out, size_t size) {
    if (num >= 1000000000ULL) {
    	snprintf(out, size, "%.*fB", 2, num / 1000000000.0);
        // snprintf(out, size, "%lluB", num / 1000000000ULL);
    } else if (num >= 1000000ULL) {
    	snprintf(out, size, "%.*fM", 2, num / 1000000.0);
        // snprintf(out, size, "%lluM", num / 1000000ULL);
    } else if (num >= 1000ULL) {
    	snprintf(out, size, "%.*fK", 2, num / 1000.0);
        // snprintf(out, size, "%lluK", num / 1000ULL);
    } else {
        snprintf(out, size, "%lu", num);
    }
}*/

void ShowBankBalance(Connection *c, const char *player_name);

static void ResourceCommandHandler(
    Connection *c,
    const char *player_name,
    const char *message,
    ResourceType type,
    const char *name
);

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

void SuperUserAccess(Connection *c,
                     const char *player_name,
                     const char *new_admin)
{
	// Only current admin may change admin
	if (!IsAdmin(c, player_name)) {
		RequestSendMail(
			c,
			player_name,
			"Unauthorized",
			"You don't have permission to grant admin access."
		);
		return;
	}

	if (new_admin[0] == '\0' || strlen(new_admin) >= sizeof(c->bot.admin_name)) {
		RequestSendMail(c, player_name, "Sudo access", "Invalid player name.");
		return;
	}

	snprintf(c->bot.admin_name,
	         sizeof(c->bot.admin_name),
	         "%s",
	         new_admin);

	RequestSendMailFmt(
		c,
		player_name,
		"Admin Updated",
		"%s is now the system administrator.",
		c->bot.admin_name
	);
}

/* Cancel the current resource transfer. */
static void StopCommandHandler(Connection *c, const char *player_name)
{
	if (c->transfer.state == TRANSFER_IDLE) {
		RequestSendMail(c, player_name, "Transfer", "No transfer in progress.");
		return;
	}

	// Only the requester or the admin may cancel
	if (strcmp(c->transfer.target_name, player_name) != 0 && !IsAdmin(c, player_name)) {
		RequestSendMailFmt(
			c,
			player_name,
			"Transfer",
			"Only %s or the admin can stop the current transfer.",
			c->transfer.target_name
		);
		return;
	}

	char target[sizeof(c->transfer.target_name)];
	snprintf(target, sizeof(target), "%s", c->transfer.target_name);

	char sent_str[20];
	format_number2(c->transfer.amount - c->transfer.remaining, sent_str, sizeof(sent_str));

	memset(&c->transfer, 0, sizeof(c->transfer));
	c->transfer.state = TRANSFER_IDLE;

	RequestSendMailFmt(
		c,
		player_name,
		"Transfer Stopped",
		"Transfer to %s cancelled (%s already sent). Marches already on the way will still arrive.",
		target,
		sent_str
	);
}

static void HelpCommandHandler(Connection *c, const char *player_name)
{
	char p = c->bot.command_prefix;

	RequestSendMailFmt(
		c,
		player_name,
		"Bot Commands",
		"%cfood <amount>, %cstone <amount>, %cwood <amount>, %core <amount>, %cgold <amount>\n"
		"  Request resources, e.g. %cfood 10M\n"
		"%cstop - cancel your current transfer\n"
		"%cstatus - show bot status\n"
		"%chelp - show this message\n"
		"Admin: %cbank bal, %csu <name>",
		p, p, p, p, p, p, p, p, p, p, p
	);
}

static void StatusCommandHandler(Connection *c, const char *player_name)
{
	const char *bank = c->bank.enabled ? "open" : "closed";

	if (c->transfer.state == TRANSFER_IDLE) {
		RequestSendMailFmt(c, player_name, "Bot Status", "Bank: %s\nTransfer: idle", bank);
		return;
	}

	const char *state;

	switch (c->transfer.state) {
		case TRANSFER_FIND_TARGET:
		case TRANSFER_WAIT_TARGET: state = "locating"; break;
		case TRANSFER_SEND_MARCH:
		case TRANSFER_WAIT_MARCH:  state = "sending"; break;
		default:                   state = "finishing"; break;
	}

	char remaining[20];
	format_number2(c->transfer.remaining, remaining, sizeof(remaining));

	RequestSendMailFmt(
		c,
		player_name,
		"Bot Status",
		"Bank: %s\nTransfer: %s %s for %s (%s remaining)",
		bank,
		state,
		GetResourceName(c->transfer.resource_type),
		c->transfer.target_name,
		remaining
	);
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

	if (MatchCommand(message, "stop") != NULL) {
		StopCommandHandler(c, player_name);
		return;
	}

	if (MatchCommand(message, "help") != NULL) {
		HelpCommandHandler(c, player_name);
		return;
	}

	if (MatchCommand(message, "status") != NULL) {
		StatusCommandHandler(c, player_name);
		return;
	}

	if (MatchCommand(message, "bank bal") != NULL) {
		ShowBankBalance(c, player_name);
		return;
	}

	if ((args = MatchCommand(message, "su")) != NULL) {
		if (args[0] == '\0') {
			RequestSendMailFmt(
				c,
				player_name,
				"Sudo access",
				"Usage: %csu <player name>",
				c->bot.command_prefix
			);
			return;
		}

		SuperUserAccess(c, player_name, args);
		return;
	}
}

static bool IsResourceAllowed(const Connection *c, ResourceType type)
{
	switch (type) {
		case RESOURCE_FOOD: return c->bank.send_food;
		case RESOURCE_ROCK: return c->bank.send_rock;
		case RESOURCE_WOOD: return c->bank.send_wood;
		case RESOURCE_ORE:  return c->bank.send_ore;
		case RESOURCE_GOLD: return c->bank.send_gold;
	}

	return false;
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
	if (!c->bank.enabled && !admin)
		return;
	
	if (!IsResourceAllowed(c, type) && !admin) {
		RequestSendMailFmt(c, player_name, "Not Available", "The bank does not send %s.", name);
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
				"Currently sending resources to %s. Use %cstop to cancel.",
				c->transfer.target_name,
				c->bot.command_prefix
			);
			
			return;
		}
	}
	
	char amount_str[32] = {0};
	
	uint64_t amount = 0;
	
	if (sscanf(message, "%31s", amount_str) == 1)
		amount = parse_number_u64(amount_str);
	
	if (amount == 0 || amount > UINT32_MAX) {
		RequestSendMailFmt(c, player_name, "Usage", "Usage: %c%s <amount>, e.g. %c%s 10M",
			c->bot.command_prefix, name, c->bot.command_prefix, name);
		return;
	}
	
	uint32_t current = 0;
	uint32_t reserve = 0;
	
	switch (type) {
		case RESOURCE_FOOD:
			current = c->resources.food;
			reserve = c->bank.reserve.food;
			break;
		case RESOURCE_ROCK:
			current = c->resources.rock;
			reserve = c->bank.reserve.rock;
			break;
		case RESOURCE_WOOD:
			current = c->resources.wood;
			reserve = c->bank.reserve.wood;
			break;
		case RESOURCE_ORE:
			current = c->resources.ore;
			reserve = c->bank.reserve.ore;
			break;
		case RESOURCE_GOLD:
			current = c->resources.gold;
			reserve = c->bank.reserve.gold;
			break;
	}
	
	if (current <= reserve || amount > (current - reserve)) {
		char curr_amount_str[20];
		
		format_number2(current > reserve ? current - reserve : 0, curr_amount_str, sizeof(curr_amount_str));
		
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



uint64_t GetBagFood(Connection *c) {
	return 
		((uint64_t)c->items[FOOD_5K].quantity * 5000) + // FOOD 5K
		((uint64_t)c->items[FOOD_30K].quantity * 30000) + // FOOD 30K
		((uint64_t)c->items[FOOD_150K].quantity * 150000) + // FOOD 150K
		((uint64_t)c->items[FOOD_500K].quantity * 500000) + // FOOD 500K
		((uint64_t)c->items[FOOD_2M].quantity * 2000000) + // FOOD 2M
		((uint64_t)c->items[FOOD_6M].quantity * 6000000) + // FOOD 6M
		((uint64_t)c->items[FOOD_20M].quantity * 20000000) + // FOOD 20M
		((uint64_t)c->items[FOOD_60M].quantity * 60000000); // FOOD 60M
}


uint64_t GetBagRock(Connection *c) {
	return 
		((uint64_t)c->items[STONE_3K].quantity   * 3000) + // STONE 3K
		((uint64_t)c->items[STONE_10K].quantity  * 10000) + // STONE 10K
		((uint64_t)c->items[STONE_50K].quantity  * 50000) + // STONE 50K
		((uint64_t)c->items[STONE_150K].quantity * 150000) + // STONE 150K
		((uint64_t)c->items[STONE_500K].quantity * 500000) + // STONE 500K
		((uint64_t)c->items[STONE_1_5M].quantity * 1500000) + // STONE 1.5M
		((uint64_t)c->items[STONE_5M].quantity   * 5000000) + // STONE 5M
		((uint64_t)c->items[STONE_15M].quantity  * 15000000); // STONE 15M
}


uint64_t GetBagWood(Connection *c) {
	return 
		((uint64_t)c->items[TIMBER_3K].quantity * 3000) + // WOOD 3K
		((uint64_t)c->items[TIMBER_10K].quantity * 10000) + // WOOD 10K
		((uint64_t)c->items[TIMBER_50K].quantity * 50000) + // WOOD 50K
		((uint64_t)c->items[TIMBER_150K].quantity * 150000) + // WOOD 150K
		((uint64_t)c->items[TIMBER_500K].quantity * 500000) + // WOOD 500K
		((uint64_t)c->items[TIMBER_1_5M].quantity * 1500000) + // WOOD 1.5M
		((uint64_t)c->items[TIMBER_5M].quantity * 5000000) + // WOOD 5M
		((uint64_t)c->items[TIMBER_15M].quantity * 15000000); // WOOD 15M
}

uint64_t GetBagOre(Connection *c) {
	return 
		((uint64_t)c->items[ORE_3K].quantity * 3000) + // ORE 3K
		((uint64_t)c->items[ORE_10K].quantity * 10000) + // ORE 10K
		((uint64_t)c->items[ORE_50K].quantity * 50000) + // ORE 50K
		((uint64_t)c->items[ORE_150K].quantity * 150000) + // ORE 150K
		((uint64_t)c->items[ORE_500K].quantity * 500000) + // ORE 500K
		((uint64_t)c->items[ORE_1_5M].quantity * 1500000) + // ORE 1.5M
		((uint64_t)c->items[ORE_5M].quantity * 5000000) + // ORE 5M
		((uint64_t)c->items[ORE_15M].quantity * 15000000); // ORE 15M
}

uint64_t GetBagGold(Connection *c) {
	return 
		((uint64_t)c->items[GOLD_3K].quantity   * 3000) + // FOOD 5K
		((uint64_t)c->items[GOLD_15K].quantity  * 15000) + // FOOD 30K
		((uint64_t)c->items[GOLD_50K].quantity  * 50000) + // FOOD 150K
		((uint64_t)c->items[GOLD_200K].quantity * 200000) + // FOOD 500K
		((uint64_t)c->items[GOLD_600K].quantity * 600000) + // FOOD 2M
		((uint64_t)c->items[GOLD_2M].quantity   * 2000000) + // FOOD 6M
		((uint64_t)c->items[GOLD_6M].quantity   * 6000000); // FOOD 20M
}


void ShowBankBalance(Connection *c, const char *player_name) {
	if (strcmp(c->bot.admin_name, player_name) != 0) {
		// Return message if necessary 
		RequestSendMail(c, player_name, "Unauthorize", "You don't have permission to see bank balance!.");
		return;
	}
	
	if (!c->items_loaded) {
		RequestSendMail(c, player_name, "Problem encounter", "Something went wrong please try again later.");
		return;
	}
	
	
	char bank_food[20];
	char bank_rock[20];
	char bank_wood[20];
	char bank_ore [20];
	char bank_gold[20];
	
	char bag_food[20];
	char bag_rock[20];
	char bag_wood[20];
	char bag_ore [20];
	char bag_gold[20];
	
	char sum_food[20];
	char sum_rock[20];
	char sum_wood[20];
	char sum_ore [20];
	char sum_gold[20];
	
	
	// Format BANK
	format_number2(c->resources.food, bank_food, sizeof(bank_food));
	format_number2(c->resources.rock, bank_rock, sizeof(bank_rock));
	format_number2(c->resources.wood, bank_wood, sizeof(bank_wood));
	format_number2(c->resources.ore,  bank_ore,  sizeof(bank_ore));
	format_number2(c->resources.gold, bank_gold, sizeof(bank_gold));
	
	uint64_t BagFood = GetBagFood(c);
	uint64_t BagRock = GetBagRock(c);
	uint64_t BagWood = GetBagWood(c);
	uint64_t BagOre = GetBagOre(c);
	uint64_t BagGold= GetBagGold(c);
	
	// Format BAG
	format_number2(BagFood, bag_food, sizeof(bag_food));
	format_number2(BagRock, bag_rock, sizeof(bag_rock));
	format_number2(BagWood, bag_wood, sizeof(bag_wood));
	format_number2(BagOre,  bag_ore,  sizeof(bag_ore));
	format_number2(BagGold, bag_gold, sizeof(bag_gold));
		
	// Format TOTAL
	format_number2(c->resources.food + BagFood,  sum_food, sizeof(sum_food));
	format_number2(c->resources.rock + BagRock,  sum_rock, sizeof(sum_rock));
	format_number2(c->resources.wood + BagWood,  sum_wood, sizeof(sum_wood));
	format_number2(c->resources.ore  + BagOre,   sum_ore,  sizeof(sum_ore));
	format_number2(c->resources.gold + BagGold,  sum_gold, sizeof(sum_gold));
	
	RequestSendMailFmt(c, player_name, "Bank Balance", 
		"[BNK] Food: %s | Stone: %s | Wood: %s | Ore: %s | Gold: %s\n"
		"[BAG] Food: %s | Stone: %s | Wood: %s | Ore: %s | Gold: %s\n"
		"[SUM] Food: %s | Stone: %s | Wood: %s | Ore: %s | Gold: %s",
		bank_food, bank_rock, bank_wood, bank_ore, bank_gold,
		bag_food, bag_rock, bag_wood, bag_ore, bag_gold,
		sum_food, sum_rock, sum_wood, sum_ore, sum_gold
	);
	return;
}