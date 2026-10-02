/*
 * Configuration parser.
 *
 * Simple on/off switches and resource amounts are listed in tables
 * (bool_options / amount_options), so adding one is a one-line change.
 * Options that need custom validation use an if/return chain.
 *
 * This runs once at startup, never in the game loop, so clarity is
 * preferred over speed.
 *
 * Unknown keys produce a warning (so typos are visible) but don't stop the
 * bot; invalid values are a hard error.
 */

#include "config.h"
#include <stdlib.h>

#include <ctype.h>
#include <stddef.h>

typedef enum {
	CONFIG_OK,
	CONFIG_UNKNOWN_KEY,
	CONFIG_INVALID
} ConfigResult;

/* Remove leading and trailing whitespace in place. */
static char *Trim(char *str)
{
	while (isspace((unsigned char)*str))
		str++;
	
	if (*str == '\0')
		return str;
	
	char *end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char)*end))
		*end-- = '\0';
	
	return str;
}

/* Parse "true"/"false" (also accepts 1/0, yes/no, on/off). */
static bool ParseBool(const char *value, bool *out)
{
	if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0 ||
	    strcmp(value, "yes") == 0  || strcmp(value, "on") == 0) {
		*out = true;
		return true;
	}
	
	if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0 ||
	    strcmp(value, "no") == 0    || strcmp(value, "off") == 0) {
		*out = false;
		return true;
	}
	
	return false;
}

/* Bounded string copy that always NUL-terminates. Fails if value doesn't fit. */
static bool CopyString(char *dst, size_t dst_size, const char *value)
{
	size_t len = strlen(value);
	
	if (len >= dst_size)
		return false;
	
	memcpy(dst, value, len + 1);
	return true;
}

uint64_t parse_number_u64(const char *str)
{
	double value = 0.0;
	char suffix = '\0';
	
	// Read numeric part and optional suffix
	if (sscanf(str, "%lf%c", &value, &suffix) < 1)
		return 0;
	
	// Apply multiplier (case-insensitive)
	switch (tolower((unsigned char)suffix)) {
		case 'k': value *= 1000.0; break;
		case 'm': value *= 1000000.0; break;
		case 'b': value *= 1000000000.0; break;
		default: break; // no suffix
	}
	
	if (value < 0) value = 0;
	
	return (uint64_t)value;
}

static bool ParseU32(const char *value, uint32_t *out)
{
	uint64_t n = parse_number_u64(value);
	
	if (n > UINT32_MAX)
		return false;
	
	*out = (uint32_t)n;
	return true;
}

static bool ParseCommandChannel(const char *value, CommandChannel *out)
{
	if (strcmp(value, "WORLD") == 0) {
		*out = COMMAND_CHANNEL_WORLD;
		return true;
	}
	
	if (strcmp(value, "GUILD") == 0) {
		*out = COMMAND_CHANNEL_GUILD;
		return true;
	}
	
	if (strcmp(value, "MAIL") == 0) {
		*out = COMMAND_CHANNEL_MAIL;
		return true;
	}
	
	return false;
}

static uint16_t ParseShield(const char *value)
{
    if (strcmp(value, "SHIELD_4H") == 0)
        return SHIELD_4H;

    if (strcmp(value, "SHIELD_8H") == 0)
        return SHIELD_8H;

    if (strcmp(value, "SHIELD_12H") == 0)
        return SHIELD_12H;

    if (strcmp(value, "SHIELD_1D") == 0)
        return SHIELD_1D;

    if (strcmp(value, "SHIELD_3D") == 0)
        return SHIELD_3D;

    if (strcmp(value, "SHIELD_7D") == 0)
        return SHIELD_7D;

    if (strcmp(value, "SHIELD_14D") == 0)
        return SHIELD_14D;

    return 0;
}

static bool ParseShieldPriority(Connection *c, const char *value)
{
	char buffer[512];
	
	if (!CopyString(buffer, sizeof(buffer), value))
		return false;
	
	const size_t max = sizeof(c->protection.shield_priority) / sizeof(c->protection.shield_priority[0]);
	size_t count = 0;
	
	char *token = strtok(buffer, ",");
	
	while (token) {
		token = Trim(token);
		
		if (count >= max) {
			printf("Too many shields in priority list (max %u)\n", (unsigned)max);
			return false;
		}
		
		uint16_t shield = ParseShield(token);
		
		if (shield == 0) {
			printf("Invalid shield priority value: %s\n", token);
			return false;
		}
		
		c->protection.shield_priority[count++] = shield;
		
		token = strtok(NULL, ",");
	}
	
	c->protection.shield_priority_count = (uint8_t)count;
	return true;
}

/*
 * Boolean options.
 *
 * Kept as a table so adding a new on/off switch is a one-line change.
 * Offsets are relative to the start of Connection.
 */
typedef struct {
	const char *key;
	size_t offset;
} BoolOption;

#define OPTION(k, field) { k, offsetof(Connection, field) }

static const BoolOption bool_options[] = {
	OPTION("alliance.auto_help",                    alliance.auto_help),
	OPTION("alliance.auto_open_gifts",              alliance.auto_open_gifts),
	
	OPTION("bank.enabled",                          bank.enabled),
	OPTION("bank.send_food",                        bank.send_food),
	OPTION("bank.send_rock",                        bank.send_rock),
	OPTION("bank.send_wood",                        bank.send_wood),
	OPTION("bank.send_ore",                         bank.send_ore),
	OPTION("bank.send_gold",                        bank.send_gold),
	OPTION("bank.use_bag_rss",                      bank.use_bag_rss),
	OPTION("bank.use_bag_food",                     bank.use_bag_food),
	OPTION("bank.use_bag_rock",                     bank.use_bag_rock),
	OPTION("bank.use_bag_wood",                     bank.use_bag_wood),
	OPTION("bank.use_bag_ore",                      bank.use_bag_ore),
	OPTION("bank.use_bag_gold",                     bank.use_bag_gold),
	
	OPTION("protection.enabled",                    protection.enabled),
	OPTION("protection.shield_always_on",           protection.shield_always_on),
	OPTION("protection.shield_on_incoming_attack",  protection.shield_on_incoming_attack),
	OPTION("protection.shield_on_incoming_scout",   protection.shield_on_incoming_scout),
	OPTION("protection.recall_on_incoming_attack",  protection.recall_on_incoming_attack),
	OPTION("protection.recall_on_incoming_scout",   protection.recall_on_incoming_scout),
	OPTION("protection.recall_on_incoming_conflict", protection.recall_on_incoming_conflict),
	
	OPTION("cargo_ship.auto_trade",                 market.settings.auto_trade),
	OPTION("cargo_ship.use_bag_rss",                market.settings.use_bag_rss),
	OPTION("cargo_ship.spend_food",                 market.settings.spend_food),
	OPTION("cargo_ship.spend_rock",                 market.settings.spend_rock),
	OPTION("cargo_ship.spend_wood",                 market.settings.spend_wood),
	OPTION("cargo_ship.spend_ore",                  market.settings.spend_ore),
	OPTION("cargo_ship.spend_gold",                 market.settings.spend_gold),
	
	OPTION("reconnect.enabled",                     reconnect.enabled),
};

/* Numeric options: resource amounts (accept K/M/B suffixes) and durations in seconds. */
typedef struct {
	const char *key;
	size_t offset;
} AmountOption;

static const AmountOption amount_options[] = {
	OPTION("bank.reserve_food",           bank.reserve.food),
	OPTION("bank.reserve_rock",           bank.reserve.rock),
	OPTION("bank.reserve_wood",           bank.reserve.wood),
	OPTION("bank.reserve_ore",            bank.reserve.ore),
	OPTION("bank.reserve_gold",           bank.reserve.gold),
	OPTION("bank.max_delivery_distance",  bank.max_delivery_distance),
	
	OPTION("cargo_ship.reserve_food",     market.reserve.food),
	OPTION("cargo_ship.reserve_rock",     market.reserve.rock),
	OPTION("cargo_ship.reserve_wood",     market.reserve.wood),
	OPTION("cargo_ship.reserve_ore",      market.reserve.ore),
	OPTION("cargo_ship.reserve_gold",     market.reserve.gold),
	
	OPTION("reconnect.delay",             reconnect.delay),
	OPTION("reconnect.max_delay",         reconnect.max_delay),
	OPTION("reconnect.max_attempts",      reconnect.max_attempts),
	OPTION("reconnect.kicked_delay",      reconnect.kicked_delay),
	OPTION("reconnect.timeout",           reconnect.timeout),
};

#undef OPTION

static ConfigResult ParserConfig(Connection *c, const char *key, const char *value)
{
	for (size_t i = 0; i < sizeof(bool_options) / sizeof(bool_options[0]); i++) {
		if (strcmp(key, bool_options[i].key) == 0) {
			bool *field = (bool *)((uint8_t *)c + bool_options[i].offset);
			return ParseBool(value, field) ? CONFIG_OK : CONFIG_INVALID;
		}
	}
	
	for (size_t i = 0; i < sizeof(amount_options) / sizeof(amount_options[0]); i++) {
		if (strcmp(key, amount_options[i].key) == 0) {
			uint32_t *field = (uint32_t *)((uint8_t *)c + amount_options[i].offset);
			return ParseU32(value, field) ? CONFIG_OK : CONFIG_INVALID;
		}
	}
	
	// gateway server 
	if (strcmp(key, "server.addr") == 0) {
		return CopyString(c->gateway_server.addr, sizeof(c->gateway_server.addr), value) ? CONFIG_OK : CONFIG_INVALID;
	}
	
	if (strcmp(key, "server.port") == 0) {
		uint32_t port;
		if (!ParseU32(value, &port) || port == 0 || port > 65535)
			return CONFIG_INVALID;
		c->gateway_server.port = port;
		return CONFIG_OK;
	}
	
	// client version 
	if (strcmp(key, "client.version_major") == 0) {
		c->app.version_major = (uint8_t)strtoul(value, NULL, 10);
		return CONFIG_OK;
	}
	
	if (strcmp(key, "client.version_minor") == 0) {
		c->app.version_minor = (uint8_t)strtoul(value, NULL, 10);
		return CONFIG_OK;
	}
	
	if (strcmp(key, "client.version_patch") == 0) {
		c->app.version_patch = (uint16_t)strtoul(value, NULL, 10);
		return CONFIG_OK;
	}
	
	if (strcmp(key, "client.language_code") == 0) {
		c->app.language_code = (uint8_t)strtoul(value, NULL, 10);
		return CONFIG_OK;
	}
	
	// data path
	if (strcmp(key, "data.path") == 0) {
		return CopyString(c->bot.data_path, sizeof(c->bot.data_path), value) ? CONFIG_OK : CONFIG_INVALID;
	}
	
	// login 
	if (strcmp(key, "account.igg_id") == 0) {
		c->auth.igg_id = (int64_t)strtoull(value, NULL, 10);
		return c->auth.igg_id > 0 ? CONFIG_OK : CONFIG_INVALID;
	}
	
	if (strcmp(key, "account.device_uuid") == 0) {
		return CopyString(c->auth.device_uuid, sizeof(c->auth.device_uuid), value) ? CONFIG_OK : CONFIG_INVALID;
	}
	
	if (strcmp(key, "account.access_key") == 0) {
		if (!CopyString(c->auth.session, sizeof(c->auth.session), value))
			return CONFIG_INVALID;
		c->auth.session_len = (uint16_t)strlen(c->auth.session);
		return CONFIG_OK;
	}
	
	// command 
	if (strcmp(key, "command.input") == 0) {
		return ParseCommandChannel(value, &c->bot.command_input) ? CONFIG_OK : CONFIG_INVALID;
	}
	
	if (strcmp(key, "command.output") == 0) {
		return ParseCommandChannel(value, &c->bot.command_output) ? CONFIG_OK : CONFIG_INVALID;
	}
	
	if (strcmp(key, "command.prefix") == 0) {
		if (value[0] == '\0' || value[1] != '\0')
			return CONFIG_INVALID;
		c->bot.command_prefix = value[0];
		return CONFIG_OK;
	}
	
	if (strcmp(key, "admin.name") == 0) {
		return CopyString(c->bot.admin_name, sizeof(c->bot.admin_name), value) ? CONFIG_OK : CONFIG_INVALID;
	}
	
	if (strcmp(key, "protection.shield_priority") == 0) {
		return ParseShieldPriority(c, value) ? CONFIG_OK : CONFIG_INVALID;
	}
	
	return CONFIG_UNKNOWN_KEY;
}

/* Defaults applied before the configuration file is read. */
static void ApplyDefaultConfig(Connection *c)
{
	CopyString(c->gateway_server.addr, sizeof(c->gateway_server.addr), "192.243.44.63");
	c->gateway_server.port = 5999;
	
	c->app.version_major = 2;
	c->app.version_minor = 197;
	c->app.version_patch = 308;
	c->app.language_code = 1;
	
	CopyString(c->bot.data_path, sizeof(c->bot.data_path), "./lmbot/");
	
	c->bot.command_prefix = '$';
	c->bot.command_input  = COMMAND_CHANNEL_GUILD;
	c->bot.command_output = COMMAND_CHANNEL_MAIL;
	
	c->bank.max_delivery_distance = 100;
	
	c->reconnect.enabled      = true;
	c->reconnect.delay        = 5;
	c->reconnect.max_delay    = 300;
	c->reconnect.max_attempts = 0;
	c->reconnect.kicked_delay = 600;
	c->reconnect.timeout      = 90;
}

/* Make sure the options required to log in are present. */
static bool ValidateConfig(const Connection *c, const char *filename)
{
	bool ok = true;
	
	if (c->auth.igg_id <= 0) {
		printf("%s: error: missing `account.igg_id`\n", filename);
		ok = false;
	}
	
	if (c->auth.device_uuid[0] == '\0') {
		printf("%s: error: missing `account.device_uuid`\n", filename);
		ok = false;
	}
	
	if (c->auth.session_len == 0 || strcmp(c->auth.session, "YOUR_ACCESS_KEY_HERE") == 0) {
		printf("%s: error: `account.access_key` is not set\n", filename);
		ok = false;
	}
	
	if (c->reconnect.delay == 0) {
		printf("%s: error: `reconnect.delay` must be at least 1\n", filename);
		ok = false;
	}
	
	if (c->reconnect.timeout != 0 && c->reconnect.timeout < 30) {
		printf("%s: error: `reconnect.timeout` must be 0 (disabled) or at least 30\n", filename);
		ok = false;
	}
	
	return ok;
}

bool LoadConfig(Connection *c, const char *filename)
{
	FILE *fp = fopen(filename, "r");
	
	if (!fp) {
		printf("%s: error: cannot open file\n", filename);
		return false;
	}
	
	ApplyDefaultConfig(c);
	
	char line[1024];
	uint32_t line_num = 0;
	
	while (fgets(line, sizeof(line), fp)) {
		line_num++;
		
		char *p = Trim(line);
		
		/* Skip blank lines and comments */
		if (*p == '\0' || *p == '#' || *p == ';' || (p[0] == '/' && p[1] == '/'))
			continue;
		
		char *eq = strchr(p, '=');
		
		if (eq == NULL) {
			printf("%s:%u: error: expected `key = value`: %s\n", filename, line_num, p);
			fclose(fp);
			return false;
		}
		
		*eq = '\0';
		
		char *key = Trim(p);
		char *value = Trim(eq + 1);
		
		if (*key == '\0') {
			printf("%s:%u: error: missing key\n", filename, line_num);
			fclose(fp);
			return false;
		}
		
		switch (ParserConfig(c, key, value)) {
			case CONFIG_OK:
				break;
			case CONFIG_UNKNOWN_KEY:
				printf("%s:%u: warning: unknown option `%s` (ignored)\n", filename, line_num, key);
				break;
			case CONFIG_INVALID:
				printf("%s:%u: error: invalid value for `%s`: `%s`\n", filename, line_num, key, value);
				fclose(fp);
				return false;
		}
	}
	
	fclose(fp);
	
	return ValidateConfig(c, filename);
}
