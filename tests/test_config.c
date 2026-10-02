/*
 * Unit tests for the configuration parser and reconnect backoff.
 *
 * Run with: ctest --test-dir build --output-on-failure
 */

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "connection.h"

int epoll_fd = 0;

static int failures = 0;

#define CHECK(cond) do { \
	if (!(cond)) { \
		printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		failures++; \
	} \
} while (0)

static const char *TEST_FILE = "test_config.tmp";

static void WriteFile(const char *text)
{
	FILE *fp = fopen(TEST_FILE, "w");
	if (!fp) {
		perror("fopen");
		exit(1);
	}
	fputs(text, fp);
	fclose(fp);
}

static Connection c;
static ProgramConfig program;

static bool LoadBot(const char *text)
{
	WriteFile(text);
	memset(&c, 0, sizeof(c));
	bool ok = LoadConfig(&c, TEST_FILE);
	remove(TEST_FILE);
	return ok;
}

static bool LoadProgram(const char *text)
{
	WriteFile(text);
	bool ok = LoadProgramConfig(TEST_FILE, &program);
	remove(TEST_FILE);
	return ok;
}

#define ACCOUNT \
	"account.igg_id = 1234567890\n" \
	"account.device_uuid = 12345678-1234-1234-1234-123456789abc\n" \
	"account.access_key = abcdef\n"

#define PROGRAM \
	"data.path = ./data/\n" \
	"include = configs/bot1.cfg\n"

static void test_parse_number(void)
{
	CHECK(parse_number_u64("0") == 0);
	CHECK(parse_number_u64("123") == 123);
	CHECK(parse_number_u64("10k") == 10000);
	CHECK(parse_number_u64("10K") == 10000);
	CHECK(parse_number_u64("1.5M") == 1500000);
	CHECK(parse_number_u64("2b") == 2000000000ULL);
	CHECK(parse_number_u64("-5") == 0);
	CHECK(parse_number_u64("abc") == 0);
}

static void test_bot_defaults(void)
{
	CHECK(LoadBot(ACCOUNT));
	CHECK(c.bot.command_prefix == '$');
	CHECK(c.bot.command_input == COMMAND_CHANNEL_GUILD);
	CHECK(c.bot.command_output == COMMAND_CHANNEL_MAIL);
	CHECK(c.bot.admin_name[0] == '\0');
	CHECK(!c.bank.enabled);
	CHECK(c.bank.max_delivery_distance == 100);
}

static void test_bot_options(void)
{
	CHECK(LoadBot(
		ACCOUNT
		"# comment\n"
		"; comment\n"
		"// comment\n"
		"\n"
		"   admin.name = Boss   \r\n"
		"command.prefix = !\n"
		"command.input = WORLD\n"
		"command.output = GUILD\n"
		"bank.enabled = true\n"
		"bank.send_food = yes\n"
		"bank.send_gold = 1\n"
		"bank.reserve_food = 20M\n"
		"bank.reserve_ore  = 1.5m\n"
		"bank.max_delivery_distance = 250\n"
		"bank.use_bag_rss = on\n"
		"alliance.auto_help = true\n"
		"protection.enabled = true\n"
		"protection.shield_always_on = true\n"
		"protection.recall_on_incoming_attack = true\n"
		"protection.shield_priority = SHIELD_8H , SHIELD_1D,SHIELD_14D\n"
		"cargo_ship.auto_trade = true\n"
		"cargo_ship.spend_wood = true\n"
		"cargo_ship.reserve_gold = 10M\n"
		"cargo_ship.trade_for_speed_up_training = true\n"
		"cargo_ship.trade_for_gold = true\n"
	));

	CHECK(strcmp(c.bot.admin_name, "Boss") == 0);
	CHECK(c.bot.command_prefix == '!');
	CHECK(c.bot.command_input == COMMAND_CHANNEL_WORLD);
	CHECK(c.bot.command_output == COMMAND_CHANNEL_GUILD);
	CHECK(c.auth.igg_id == 1234567890);
	CHECK(c.auth.session_len == 6);

	CHECK(c.bank.enabled);
	CHECK(c.bank.allowed[RESOURCE_FOOD]);
	CHECK(!c.bank.allowed[RESOURCE_ROCK]);
	CHECK(c.bank.allowed[RESOURCE_GOLD]);
	CHECK(c.bank.reserve[RESOURCE_FOOD] == 20000000);
	CHECK(c.bank.reserve[RESOURCE_ORE] == 1500000);
	CHECK(c.bank.max_delivery_distance == 250);
	CHECK(c.bank.use_bag_rss);

	CHECK(c.alliance.auto_help);

	CHECK(c.protection.enabled);
	CHECK(c.protection.shield_always_on);
	CHECK(c.protection.recall_on_incoming_attack);
	CHECK(c.protection.shield_priority_count == 3);
	CHECK(c.protection.shield_priority[0] == SHIELD_8H);
	CHECK(c.protection.shield_priority[1] == SHIELD_1D);
	CHECK(c.protection.shield_priority[2] == SHIELD_14D);

	CHECK(c.cargo_ship.settings.auto_trade);
	CHECK(c.cargo_ship.settings.spend[RESOURCE_WOOD]);
	CHECK(!c.cargo_ship.settings.spend[RESOURCE_FOOD]);
	CHECK(c.cargo_ship.reserve[RESOURCE_GOLD] == 10000000);
	CHECK(c.cargo_ship.trade_for.speed_up_training);
	CHECK(c.cargo_ship.trade_for.gold);
}

static void test_shield_always_on_does_not_touch_enabled(void)
{
	// shield_always_on used to overwrite protection.enabled
	CHECK(LoadBot(ACCOUNT
		"protection.enabled = false\n"
		"protection.shield_always_on = true\n"));
	CHECK(!c.protection.enabled);
	CHECK(c.protection.shield_always_on);
}

static void test_bot_invalid_values(void)
{
	CHECK(!LoadBot(ACCOUNT "bank.enabled = maybe\n"));
	CHECK(!LoadBot(ACCOUNT "command.input = CHAT\n"));
	CHECK(!LoadBot(ACCOUNT "command.prefix = !!\n"));
	CHECK(!LoadBot(ACCOUNT "protection.shield_priority = SHIELD_2H\n"));
	CHECK(!LoadBot(ACCOUNT "admin.name = ThisNameIsWayTooLong\n"));
	CHECK(!LoadBot(ACCOUNT "bank.reserve_food = 10B\n"));
	CHECK(!LoadBot(ACCOUNT "this line has no equals sign\n"));
	CHECK(!LoadBot(ACCOUNT
		"protection.shield_priority = SHIELD_4H,SHIELD_4H,SHIELD_4H,SHIELD_4H,"
		"SHIELD_4H,SHIELD_4H,SHIELD_4H,SHIELD_4H,SHIELD_4H\n"));

	// unknown keys only warn
	CHECK(LoadBot(ACCOUNT "some.future_option = 1\n"));
}

static void test_bot_missing_account(void)
{
	CHECK(!LoadBot("bank.enabled = true\n"));
	CHECK(!LoadBot(
		"account.igg_id = 1234567890\n"
		"account.device_uuid = YOUR_DEVICE_UUID_HERE\n"
		"account.access_key = abc\n"));
	CHECK(!LoadBot(
		"account.igg_id = 1234567890\n"
		"account.device_uuid = 12345678-1234-1234-1234-123456789abc\n"
		"account.access_key = YOUR_ACCESS_KEY_HERE\n"));

	// The generated example config must be rejected until it is filled in.
	remove("generated.tmp");
	CHECK(CreateDefaultConfig("generated.tmp"));
	CHECK(!CreateDefaultConfig("generated.tmp")); // never overwrite
	memset(&c, 0, sizeof(c));
	CHECK(!LoadConfig(&c, "generated.tmp"));
	remove("generated.tmp");
}

static void test_program_config(void)
{
	CHECK(LoadProgram(PROGRAM));
	CHECK(strcmp(program.server_addr, "192.243.44.63") == 0);
	CHECK(program.server_port == 5999);
	CHECK(program.version_major == 2);
	CHECK(program.bot_count == 1);
	CHECK(strcmp(program.config_path[0], "configs/bot1.cfg") == 0);
	CHECK(program.reconnect.enabled);
	CHECK(program.reconnect.delay == 5);
	CHECK(program.reconnect.max_delay == 300);
	CHECK(program.reconnect.max_attempts == 0);
	CHECK(program.reconnect.kicked_delay == 600);
	CHECK(program.reconnect.timeout == 90);

	CHECK(LoadProgram(
		"server.addr = 10.0.0.1\n"
		"server.port = 6000\n"
		"client.version_patch = 400\n"
		"data.path = /tmp/lmbot/\n"
		"include = configs/a.cfg\n"
		"include = configs/b.cfg\n"
		"reconnect.enabled = false\n"
		"reconnect.delay = 2\n"
		"reconnect.max_delay = 60\n"
		"reconnect.max_attempts = 10\n"
		"reconnect.kicked_delay = 1200\n"
		"reconnect.timeout = 0\n"));
	CHECK(strcmp(program.server_addr, "10.0.0.1") == 0);
	CHECK(program.server_port == 6000);
	CHECK(program.version_patch == 400);
	CHECK(strcmp(program.data_path, "/tmp/lmbot/") == 0);
	CHECK(program.bot_count == 2);
	CHECK(strcmp(program.config_path[1], "configs/b.cfg") == 0);
	CHECK(!program.reconnect.enabled);
	CHECK(program.reconnect.delay == 2);
	CHECK(program.reconnect.max_delay == 60);
	CHECK(program.reconnect.max_attempts == 10);
	CHECK(program.reconnect.kicked_delay == 1200);
	CHECK(program.reconnect.timeout == 0);

	// The generated program.cfg must load as is.
	remove("generated.tmp");
	CHECK(CreateDefaultProgramConfig("generated.tmp"));
	CHECK(!CreateDefaultProgramConfig("generated.tmp")); // never overwrite
	CHECK(LoadProgramConfig("generated.tmp", &program));
	remove("generated.tmp");
}

static void test_program_invalid_values(void)
{
	CHECK(!LoadProgram("data.path = ./data/\n"));                  // no bots
	CHECK(!LoadProgram("include = configs/bot1.cfg\n"));           // no data path
	CHECK(!LoadProgram(PROGRAM "server.port = 70000\n"));
	CHECK(!LoadProgram(PROGRAM "server.port = 0\n"));
	CHECK(!LoadProgram(PROGRAM "server.port = 59x\n"));
	CHECK(!LoadProgram(PROGRAM "server.addr = 255.255.255.255.255\n"));
	CHECK(!LoadProgram(PROGRAM "client.version_minor = 300\n"));
	CHECK(!LoadProgram(PROGRAM "reconnect.enabled = sometimes\n"));
	CHECK(!LoadProgram(PROGRAM "reconnect.delay = 0\n"));
	CHECK(!LoadProgram(PROGRAM "reconnect.timeout = 10\n"));
	CHECK(!LoadProgram(PROGRAM "reconnect.max_attempts = -1\n"));
}

static void test_reconnect_delay(void)
{
	ReconnectSettings s = { .delay = 5, .max_delay = 300 };

	CHECK(ReconnectDelay(&s, 1) == 5);
	CHECK(ReconnectDelay(&s, 2) == 10);
	CHECK(ReconnectDelay(&s, 3) == 20);
	CHECK(ReconnectDelay(&s, 6) == 160);
	CHECK(ReconnectDelay(&s, 7) == 300);
	CHECK(ReconnectDelay(&s, 1000) == 300);

	// max_delay below delay: never go under delay
	ReconnectSettings low = { .delay = 30, .max_delay = 10 };
	CHECK(ReconnectDelay(&low, 1) == 30);
	CHECK(ReconnectDelay(&low, 5) == 30);

	// huge values must not overflow
	ReconnectSettings big = { .delay = 3000000000u, .max_delay = 4000000000u };
	CHECK(ReconnectDelay(&big, 2) == 4000000000u);
	CHECK(ReconnectDelay(&big, 50) == 4000000000u);
}

int main(void)
{
	test_parse_number();
	test_bot_defaults();
	test_bot_options();
	test_shield_always_on_does_not_touch_enabled();
	test_bot_invalid_values();
	test_bot_missing_account();
	test_program_config();
	test_program_invalid_values();
	test_reconnect_delay();

	if (failures) {
		printf("%d check(s) failed\n", failures);
		return 1;
	}

	printf("All config tests passed\n");
	return 0;
}
