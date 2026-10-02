/*
 * Configuration parser.
 *
 * Two kinds of files share one parser:
 *   - program.cfg: global settings (server, client version, reconnect, bot list)
 *   - one file per bot: account, commands, bank, alliance, protection, cargo ship
 *
 * Simple options are listed in tables (bot_options), so adding one is a
 * one-line change. Options that need custom validation use an if/return chain.
 *
 * This runs once at startup, never in the game loop, so clarity is
 * preferred over speed.
 *
 * Unknown keys produce a warning (so typos are visible) but don't stop the
 * bot; invalid values are a hard error that names the file and line.
 */

#include "config.h"
#include <stdlib.h>

#include <ctype.h>
#include <stddef.h>

#include "log.h"
#include "connection.h"

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

/* Plain integer with an upper bound (no K/M/B suffix). */
static bool ParseUInt(const char *value, uint32_t max, uint32_t *out)
{
	char *end;

	if (!isdigit((unsigned char)value[0]))
		return false;

	unsigned long n = strtoul(value, &end, 10);

	if (*end != '\0' || n > max)
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
	if (strcmp(value, "SHIELD_4H") == 0)  return SHIELD_4H;
	if (strcmp(value, "SHIELD_8H") == 0)  return SHIELD_8H;
	if (strcmp(value, "SHIELD_12H") == 0) return SHIELD_12H;
	if (strcmp(value, "SHIELD_1D") == 0)  return SHIELD_1D;
	if (strcmp(value, "SHIELD_3D") == 0)  return SHIELD_3D;
	if (strcmp(value, "SHIELD_7D") == 0)  return SHIELD_7D;
	if (strcmp(value, "SHIELD_14D") == 0) return SHIELD_14D;

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
			LOGE("Too many shields in priority list (max %u)\n", (unsigned)max);
			return false;
		}

		uint16_t shield = ParseShield(token);

		if (shield == 0) {
			LOGE("Invalid shield priority value: %s\n", token);
			return false;
		}

		c->protection.shield_priority[count++] = shield;

		token = strtok(NULL, ",");
	}

	c->protection.shield_priority_count = (uint8_t)count;
	return true;
}

/*
 * Read `key = value` lines from a file and pass each pair to `handler`.
 * Blank lines and lines starting with #, ; or // are skipped.
 */
typedef ConfigResult (*ConfigHandler)(void *ctx, const char *key, const char *value);

static bool ParseConfigFile(const char *filename, ConfigHandler handler, void *ctx)
{
	FILE *fp = fopen(filename, "r");

	if (!fp) {
		LOGE("%s: cannot open file\n", filename);
		return false;
	}

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
			LOGE("%s:%u: expected `key = value`: %s\n", filename, line_num, p);
			fclose(fp);
			return false;
		}

		*eq = '\0';

		char *key = Trim(p);
		char *value = Trim(eq + 1);

		if (*key == '\0') {
			LOGE("%s:%u: missing key\n", filename, line_num);
			fclose(fp);
			return false;
		}

		switch (handler(ctx, key, value)) {
			case CONFIG_OK:
				break;
			case CONFIG_UNKNOWN_KEY:
				LOGW("%s:%u: unknown option `%s` (ignored)\n", filename, line_num, key);
				break;
			case CONFIG_INVALID:
				LOGE("%s:%u: invalid value for `%s`: `%s`\n", filename, line_num, key, value);
				fclose(fp);
				return false;
		}
	}

	fclose(fp);
	return true;
}

/* ------------------------------------------------------------------------- */
/* Bot configuration                                                         */
/* ------------------------------------------------------------------------- */

typedef enum {
	OPT_BOOL,   // bool
	OPT_FLAG,   // uint8_t used as a boolean
	OPT_AMOUNT  // uint32_t, accepts K/M/B suffixes
} OptionType;

typedef struct {
	const char *key;
	OptionType type;
	size_t offset;
} BotOption;

#define OPTION(k, t, field) { k, t, offsetof(Connection, field) }

static const BotOption bot_options[] = {
	OPTION("bank.enabled",                           OPT_BOOL,   bank.enabled),
	OPTION("bank.send_food",                         OPT_FLAG,   bank.allowed[0]),
	OPTION("bank.send_rock",                         OPT_FLAG,   bank.allowed[1]),
	OPTION("bank.send_wood",                         OPT_FLAG,   bank.allowed[2]),
	OPTION("bank.send_ore",                          OPT_FLAG,   bank.allowed[3]),
	OPTION("bank.send_gold",                         OPT_FLAG,   bank.allowed[4]),
	OPTION("bank.reserve_food",                      OPT_AMOUNT, bank.reserve[0]),
	OPTION("bank.reserve_rock",                      OPT_AMOUNT, bank.reserve[1]),
	OPTION("bank.reserve_wood",                      OPT_AMOUNT, bank.reserve[2]),
	OPTION("bank.reserve_ore",                       OPT_AMOUNT, bank.reserve[3]),
	OPTION("bank.reserve_gold",                      OPT_AMOUNT, bank.reserve[4]),
	OPTION("bank.max_delivery_distance",             OPT_AMOUNT, bank.max_delivery_distance),
	OPTION("bank.use_bag_rss",                       OPT_BOOL,   bank.use_bag_rss),
	OPTION("bank.use_bag_food",                      OPT_BOOL,   bank.use_bag_food),
	OPTION("bank.use_bag_rock",                      OPT_BOOL,   bank.use_bag_rock),
	OPTION("bank.use_bag_wood",                      OPT_BOOL,   bank.use_bag_wood),
	OPTION("bank.use_bag_ore",                       OPT_BOOL,   bank.use_bag_ore),
	OPTION("bank.use_bag_gold",                      OPT_BOOL,   bank.use_bag_gold),

	OPTION("alliance.auto_help",                     OPT_BOOL,   alliance.auto_help),
	OPTION("alliance.auto_open_gifts",               OPT_BOOL,   alliance.auto_open_gifts),

	OPTION("protection.enabled",                     OPT_BOOL,   protection.enabled),
	OPTION("protection.shield_always_on",            OPT_BOOL,   protection.shield_always_on),
	OPTION("protection.shield_on_incoming_attack",   OPT_BOOL,   protection.shield_on_incoming_attack),
	OPTION("protection.shield_on_incoming_scout",    OPT_BOOL,   protection.shield_on_incoming_scout),
	OPTION("protection.recall_on_incoming_attack",   OPT_BOOL,   protection.recall_on_incoming_attack),
	OPTION("protection.recall_on_incoming_scout",    OPT_BOOL,   protection.recall_on_incoming_scout),
	OPTION("protection.recall_on_incoming_conflict", OPT_BOOL,   protection.recall_on_incoming_conflict),

	OPTION("cargo_ship.auto_trade",                  OPT_BOOL,   cargo_ship.settings.auto_trade),
	OPTION("cargo_ship.use_bag_rss",                 OPT_BOOL,   cargo_ship.settings.use_bag_rss),
	OPTION("cargo_ship.spend_food",                  OPT_BOOL,   cargo_ship.settings.spend[0]),
	OPTION("cargo_ship.spend_rock",                  OPT_BOOL,   cargo_ship.settings.spend[1]),
	OPTION("cargo_ship.spend_wood",                  OPT_BOOL,   cargo_ship.settings.spend[2]),
	OPTION("cargo_ship.spend_ore",                   OPT_BOOL,   cargo_ship.settings.spend[3]),
	OPTION("cargo_ship.spend_gold",                  OPT_BOOL,   cargo_ship.settings.spend[4]),
	OPTION("cargo_ship.reserve_food",                OPT_AMOUNT, cargo_ship.reserve[0]),
	OPTION("cargo_ship.reserve_rock",                OPT_AMOUNT, cargo_ship.reserve[1]),
	OPTION("cargo_ship.reserve_wood",                OPT_AMOUNT, cargo_ship.reserve[2]),
	OPTION("cargo_ship.reserve_ore",                 OPT_AMOUNT, cargo_ship.reserve[3]),
	OPTION("cargo_ship.reserve_gold",                OPT_AMOUNT, cargo_ship.reserve[4]),

	OPTION("cargo_ship.trade_for_blazing_ember",     OPT_BOOL,   cargo_ship.trade_for.blazing_ember),
	OPTION("cargo_ship.trade_for_archaic_tome",      OPT_BOOL,   cargo_ship.trade_for.archaic_tome),
	OPTION("cargo_ship.trade_for_bright_talent_orb", OPT_BOOL,   cargo_ship.trade_for.bright_talent_orb),
	OPTION("cargo_ship.trade_for_exp_elixir",        OPT_BOOL,   cargo_ship.trade_for.exp_elixir),
	OPTION("cargo_ship.trade_for_speed_up",          OPT_BOOL,   cargo_ship.trade_for.speed_up),
	OPTION("cargo_ship.trade_for_speed_up_research", OPT_BOOL,   cargo_ship.trade_for.speed_up_research),
	OPTION("cargo_ship.trade_for_speed_up_merging",  OPT_BOOL,   cargo_ship.trade_for.speed_up_merging),
	OPTION("cargo_ship.trade_for_speed_up_training", OPT_BOOL,   cargo_ship.trade_for.speed_up_training),
	OPTION("cargo_ship.trade_for_star_scroll",       OPT_BOOL,   cargo_ship.trade_for.star_scroll),
	OPTION("cargo_ship.trade_for_wolfpack_sigil",    OPT_BOOL,   cargo_ship.trade_for.wolfpack_sigil),
	OPTION("cargo_ship.trade_for_anima",             OPT_BOOL,   cargo_ship.trade_for.anima),
	OPTION("cargo_ship.trade_for_food",              OPT_BOOL,   cargo_ship.trade_for.food),
	OPTION("cargo_ship.trade_for_rock",              OPT_BOOL,   cargo_ship.trade_for.rock),
	OPTION("cargo_ship.trade_for_wood",              OPT_BOOL,   cargo_ship.trade_for.wood),
	OPTION("cargo_ship.trade_for_ore",               OPT_BOOL,   cargo_ship.trade_for.ore),
	OPTION("cargo_ship.trade_for_gold",              OPT_BOOL,   cargo_ship.trade_for.gold),
};

#undef OPTION

static ConfigResult ParseBotOption(void *ctx, const char *key, const char *value)
{
	Connection *c = ctx;

	for (size_t i = 0; i < sizeof(bot_options) / sizeof(bot_options[0]); i++) {
		const BotOption *opt = &bot_options[i];

		if (strcmp(key, opt->key) != 0)
			continue;

		void *field = (uint8_t *)c + opt->offset;
		bool b;

		switch (opt->type) {
			case OPT_BOOL:
				return ParseBool(value, (bool *)field) ? CONFIG_OK : CONFIG_INVALID;
			case OPT_FLAG:
				if (!ParseBool(value, &b))
					return CONFIG_INVALID;
				*(uint8_t *)field = b;
				return CONFIG_OK;
			case OPT_AMOUNT:
				return ParseU32(value, (uint32_t *)field) ? CONFIG_OK : CONFIG_INVALID;
		}
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

bool LoadConfig(Connection *c, const char *filename)
{
	// Defaults
	c->bot.command_prefix = '$';
	c->bot.command_input  = COMMAND_CHANNEL_GUILD;
	c->bot.command_output = COMMAND_CHANNEL_MAIL;
	c->bank.max_delivery_distance = 100;

	if (!ParseConfigFile(filename, ParseBotOption, c))
		return false;

	// The options needed to log in must be present.
	bool ok = true;

	if (c->auth.igg_id <= 0) {
		LOGE("%s: missing `account.igg_id`\n", filename);
		ok = false;
	}

	if (c->auth.device_uuid[0] == '\0' || strcmp(c->auth.device_uuid, "YOUR_DEVICE_UUID_HERE") == 0) {
		LOGE("%s: `account.device_uuid` is not set\n", filename);
		ok = false;
	}

	if (c->auth.session_len == 0 || strcmp(c->auth.session, "YOUR_ACCESS_KEY_HERE") == 0) {
		LOGE("%s: `account.access_key` is not set\n", filename);
		ok = false;
	}

	return ok;
}

/* ------------------------------------------------------------------------- */
/* Program configuration                                                     */
/* ------------------------------------------------------------------------- */

static ConfigResult ParseProgramOption(void *ctx, const char *key, const char *value)
{
	ProgramConfig *p = ctx;
	uint32_t n;

	// gateway server
	if (strcmp(key, "server.addr") == 0) {
		return CopyString(p->server_addr, sizeof(p->server_addr), value) ? CONFIG_OK : CONFIG_INVALID;
	}

	if (strcmp(key, "server.port") == 0) {
		if (!ParseUInt(value, 65535, &n) || n == 0)
			return CONFIG_INVALID;
		p->server_port = (uint16_t)n;
		return CONFIG_OK;
	}

	// client version
	if (strcmp(key, "client.version_major") == 0) {
		if (!ParseUInt(value, UINT8_MAX, &n))
			return CONFIG_INVALID;
		p->version_major = (uint8_t)n;
		return CONFIG_OK;
	}

	if (strcmp(key, "client.version_minor") == 0) {
		if (!ParseUInt(value, UINT8_MAX, &n))
			return CONFIG_INVALID;
		p->version_minor = (uint8_t)n;
		return CONFIG_OK;
	}

	if (strcmp(key, "client.version_patch") == 0) {
		if (!ParseUInt(value, UINT16_MAX, &n))
			return CONFIG_INVALID;
		p->version_patch = (uint16_t)n;
		return CONFIG_OK;
	}

	if (strcmp(key, "client.language_code") == 0) {
		if (!ParseUInt(value, UINT8_MAX, &n))
			return CONFIG_INVALID;
		p->language_code = (uint8_t)n;
		return CONFIG_OK;
	}

	// data path
	if (strcmp(key, "data.path") == 0) {
		return CopyString(p->data_path, sizeof(p->data_path), value) ? CONFIG_OK : CONFIG_INVALID;
	}

	// include bots
	if (strcmp(key, "include") == 0) {
		const size_t max = sizeof(p->config_path) / sizeof(p->config_path[0]);

		if (p->bot_count >= max) {
			LOGE("Maximum number of bot configurations (%u) exceeded\n", (unsigned)max);
			return CONFIG_INVALID;
		}

		if (!CopyString(p->config_path[p->bot_count], sizeof(p->config_path[0]), value))
			return CONFIG_INVALID;

		p->bot_count++;
		return CONFIG_OK;
	}

	// auto reconnect
	if (strcmp(key, "reconnect.enabled") == 0)
		return ParseBool(value, &p->reconnect.enabled) ? CONFIG_OK : CONFIG_INVALID;
	if (strcmp(key, "reconnect.delay") == 0)
		return ParseUInt(value, 86400, &p->reconnect.delay) ? CONFIG_OK : CONFIG_INVALID;
	if (strcmp(key, "reconnect.max_delay") == 0)
		return ParseUInt(value, 86400, &p->reconnect.max_delay) ? CONFIG_OK : CONFIG_INVALID;
	if (strcmp(key, "reconnect.max_attempts") == 0)
		return ParseUInt(value, UINT32_MAX, &p->reconnect.max_attempts) ? CONFIG_OK : CONFIG_INVALID;
	if (strcmp(key, "reconnect.kicked_delay") == 0)
		return ParseUInt(value, 86400, &p->reconnect.kicked_delay) ? CONFIG_OK : CONFIG_INVALID;
	if (strcmp(key, "reconnect.timeout") == 0)
		return ParseUInt(value, 86400, &p->reconnect.timeout) ? CONFIG_OK : CONFIG_INVALID;

	return CONFIG_UNKNOWN_KEY;
}

bool LoadProgramConfig(const char *filepath, ProgramConfig *program)
{
	memset(program, 0, sizeof(*program));

	// Defaults
	CopyString(program->server_addr, sizeof(program->server_addr), "192.243.44.63");
	program->server_port   = 5999;
	program->version_major = 2;
	program->version_minor = 197;
	program->version_patch = 308;
	program->language_code = 1;

	program->reconnect.enabled      = true;
	program->reconnect.delay        = 5;
	program->reconnect.max_delay    = 300;
	program->reconnect.max_attempts = 0;
	program->reconnect.kicked_delay = 600;
	program->reconnect.timeout      = 90;

	if (!ParseConfigFile(filepath, ParseProgramOption, program))
		return false;

	bool ok = true;

	if (program->bot_count == 0) {
		LOGE("%s: no bot configurations (add `include = configs/<bot>.cfg`)\n", filepath);
		ok = false;
	}

	if (program->data_path[0] == '\0') {
		LOGE("%s: missing `data.path`\n", filepath);
		ok = false;
	}

	if (program->reconnect.delay == 0) {
		LOGE("%s: `reconnect.delay` must be at least 1\n", filepath);
		ok = false;
	}

	if (program->reconnect.timeout != 0 && program->reconnect.timeout < 30) {
		LOGE("%s: `reconnect.timeout` must be 0 (disabled) or at least 30\n", filepath);
		ok = false;
	}

	return ok;
}

/* ------------------------------------------------------------------------- */
/* Default files                                                             */
/* ------------------------------------------------------------------------- */

/* Never overwrite an existing configuration (it may contain account credentials). */
static bool FileExists(const char *filename)
{
	FILE *fp = fopen(filename, "r");

	if (!fp)
		return false;

	fclose(fp);
	return true;
}

bool CreateDefaultProgramConfig(const char *filename)
{
	if (FileExists(filename)) {
		LOGE("%s already exists; delete or rename it first.\n", filename);
		return false;
	}
	
	FILE *fp = fopen(filename, "w");

	if (!fp) {
		return false;
	}

	fprintf(fp,
		"# Lords Mobile Bot - Program Configuration\n"
		"# This file contains global application settings.\n"
		"# Individual bot settings are stored in separate configuration files.\n"
		"#\n"
		"# GitHub: https://github.com/halloweeks/lords-mobile-bot\n"
		"# Documentation: https://github.com/halloweeks/lords-mobile-bot/blob/main/docs/configuration.md\n\n"

		"# Gateway server\n"
		"server.addr = 192.243.44.63\n"
		"server.port = 5999\n\n"

		"# Game client version\n"
		"client.version_major = 2\n"
		"client.version_minor = 197\n"
		"client.version_patch = 308\n"
		"client.language_code = 1\n\n"
		
		"# Auto reconnect\n"
		"# Reconnect each bot automatically when its connection is lost.\n"
		"# An outdated client version or expired access key never triggers a reconnect.\n"
		"reconnect.enabled = true\n"
		"# First retry delay in seconds; doubles after each failure up to max_delay.\n"
		"reconnect.delay = 5\n"
		"reconnect.max_delay = 300\n"
		"# Give up after this many failed attempts in a row (0 = never give up).\n"
		"reconnect.max_attempts = 0\n"
		"# Wait this long when the account is logged in from another device,\n"
		"# so the bot does not keep kicking you off your phone.\n"
		"reconnect.kicked_delay = 600\n"
		"# Treat the connection as dead after this many seconds without data (0 = off).\n"
		"reconnect.timeout = 90\n\n"
		
		"# Directory used to store bot data (logs, databases, cache, etc.)\n"
		"data.path = lords-mobile-bot/data/\n\n"

		"# Load one or more bot configuration files.\n"
		"include = configs/bank123.cfg\n"
		"# include = configs/farm01.cfg\n"
	);

	fclose(fp);
	return true;
}

bool CreateDefaultConfig(const char *filename)
{
	if (FileExists(filename)) {
		LOGE("%s already exists; delete or rename it first.\n", filename);
		return false;
	}
	
	FILE *fp = fopen(filename, "w");
	
	if (!fp)
		return false;
	
	fprintf(fp,
		"# Lords Mobile Bot - Bot Configuration\n"
		"# This file contains settings for a single bot account.\n"
		"# Copy this file to create additional bot configurations.\n"
		"#\n"
		"# GitHub: https://github.com/halloweeks/lords-mobile-bot\n"
		"# Documentation: https://github.com/halloweeks/lords-mobile-bot/blob/main/docs/configuration.md\n\n"
		
		"# Privileged player.\n"
		"# This player can execute administrator commands and bypass normal restrictions.\n"
		"# Leave empty to disable admin commands.\n"
		"admin.name = \n\n"
		
		"# Replace the example values below with your own account information.\n"
		"account.igg_id      = YOUR_NUMERIC_ID_HERE\n"
		"account.device_uuid = YOUR_DEVICE_UUID_HERE\n"
		"account.access_key  = YOUR_ACCESS_KEY_HERE\n\n"
		
		"# Prefix used to identify bot commands.\n"
		"command.prefix = $\n\n"
		
		"# Command channels: WORLD, GUILD, MAIL\n"
		"command.input  = GUILD\n"
		"command.output = MAIL\n\n"
		
		"# Bank\n"
		"# Master switch for the banking system.\n"
		"# When enabled, the bot accepts and processes banking commands.\n"
		"# When disabled, all banking commands are ignored.\n"
		"bank.enabled = false\n\n"
		
		"# Resource types allowed for delivery.\n"
		"bank.send_food = false\n"
		"bank.send_rock = false\n"
		"bank.send_wood = false\n"
		"bank.send_ore  = false\n"
		"bank.send_gold = false\n\n"
		
		"# Resource reserve.\n"
		"# These values are reserved for the bot's own use. The bot will not send\n"
		"# resources that would reduce the balance below these amounts.\n"
		"bank.reserve_food = 20M\n"
		"bank.reserve_rock = 50M\n"
		"bank.reserve_wood = 50M\n"
		"bank.reserve_ore  = 30M\n"
		"bank.reserve_gold = 0\n\n"
		
		"# Maximum map distance (tiles) for resource delivery.\n"
		"bank.max_delivery_distance = 100\n\n"
		
		"# Automatically use resource items from the bag if the available\n"
		"# resources are insufficient to fulfill a banking command.\n"
		"bank.use_bag_rss  = false\n"
		"bank.use_bag_food = false\n"
		"bank.use_bag_rock = false\n"
		"bank.use_bag_wood = false\n"
		"bank.use_bag_ore  = false\n"
		"bank.use_bag_gold = false\n\n"
		
		"# Alliance\n"
		"# Automatically send Guild Help to guild members whenever available.\n"
		"# Helping guild members reduces their timers and earns Guild Coins.\n"
		"alliance.auto_help = false\n\n"
		
		"# Automatically open Alliance Gift chests and claim all available rewards.\n"
		"alliance.auto_open_gifts = false\n\n"
		
		"# Enable or disable all automatic protection features.\n"
		"protection.enabled = false\n\n"
		
		"# Keep a shield active at all times.\n"
		"protection.shield_always_on = false\n\n"
		
		"# Automatically use a shield when an incoming attack is detected.\n"
		"protection.shield_on_incoming_attack = false\n\n"
		
		"# Automatically use a shield when an incoming scout is detected.\n"
		"protection.shield_on_incoming_scout = false\n\n"
		
		"# Shield priority list.\n"
		"# The bot will use the first available shield in this order.\n"
		"# Available shields:\n"
		"# SHIELD_4H, SHIELD_8H, SHIELD_12H, SHIELD_1D, SHIELD_3D, SHIELD_7D, SHIELD_14D\n"
		"protection.shield_priority = SHIELD_4H, SHIELD_8H, SHIELD_12H, SHIELD_1D\n"
		
		"\n\n# Automatically recall marches when an incoming attack is detected.\n"
		"protection.recall_on_incoming_attack = false\n\n"
		
		"# Automatically recall marches when an incoming scout is detected.\n"
		"protection.recall_on_incoming_scout = false\n\n"
		
		"# Automatically recalls a gathering or camp march before it reaches\n"
		"# its destination when an incoming conflict is detected.\n"
		"# Requires Withdraw Squad items; otherwise no action is taken.\n"
		"protection.recall_on_incoming_conflict = false\n\n"
		
		"# Currently this feature not available\n"
		"# protection.shelter_always = false\n"
		"# protection.shelter_leader = true\n"
		"# protection.shelter_troops = false\n"
		"# protection.shelter_on_incoming_attack = true\n"
		"# protection.shelter_on_incoming_scout = true\n\n"
		
		
		"# Cargo Ship Trading\n"
		"# Automatically completes Cargo Ship trades.\n"
		"cargo_ship.auto_trade  = false\n"
		"# The options below specify which resources the bot is allowed to spend.\n"
		"cargo_ship.spend_food  = false\n"
		"cargo_ship.spend_rock  = false\n"
		"cargo_ship.spend_wood  = false\n"
		"cargo_ship.spend_ore   = false\n"
		"cargo_ship.spend_gold  = false\n\n"
		
		"# Use resource items from the bag when required to complete a trade.\n"
		"# If disabled, the bot will never consume bag resource items.\n"
		"cargo_ship.use_bag_rss = false\n\n"
		
		"# Resource reserve limits.\n"
		"# The bot always keeps at least this amount and only spends the excess.\n"
		"# Set to 0 to disable the reserve.\n"
		"cargo_ship.reserve_food = 10M\n"
		"cargo_ship.reserve_rock = 10M\n"
		"cargo_ship.reserve_wood = 10M\n"
		"cargo_ship.reserve_ore  = 10M\n"
		"cargo_ship.reserve_gold = 10M\n\n"
		
		"# Items the bot will automatically trade for when enabled (true)\n"
		"cargo_ship.trade_for_archaic_tome       = false\n"
		"cargo_ship.trade_for_bright_talent_orb  = false\n"
		"cargo_ship.trade_for_exp_elixir         = false\n"
		"cargo_ship.trade_for_speed_up           = false\n"
		"cargo_ship.trade_for_speed_up_research  = false\n"
		"cargo_ship.trade_for_blazing_ember      = false\n\n"
		"cargo_ship.trade_for_speed_up_training  = false\n"
		"cargo_ship.trade_for_speed_up_merging   = false\n"
		"cargo_ship.trade_for_anima = false\n\n"
		"# Trades resources\n"
		"cargo_ship.trade_for_food  = false\n"
		"cargo_ship.trade_for_rock  = false\n"
		"cargo_ship.trade_for_wood  = false\n"
		"cargo_ship.trade_for_ore   = false\n"
		"cargo_ship.trade_for_gold  = false\n\n"
	);

    fclose(fp);
    return true;
}