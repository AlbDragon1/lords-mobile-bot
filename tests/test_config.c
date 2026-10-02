/*
 * Unit tests for the configuration parser.
 *
 * Run with: ctest --test-dir build --output-on-failure
 */

#include <stdio.h>
#include <stdlib.h>

#include "config.h"

static int failures = 0;

#define CHECK(cond) do { \
	if (!(cond)) { \
		printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		failures++; \
	} \
} while (0)

static const char *TEST_FILE = "test_config.tmp";

static bool LoadFromString(Connection *c, const char *text)
{
	FILE *fp = fopen(TEST_FILE, "w");
	if (!fp) {
		perror("fopen");
		exit(1);
	}
	fputs(text, fp);
	fclose(fp);
	
	memset(c, 0, sizeof(*c));
	bool ok = LoadConfig(c, TEST_FILE);
	remove(TEST_FILE);
	return ok;
}

#define ACCOUNT \
	"account.igg_id = 1234567890\n" \
	"account.device_uuid = 12345678-1234-1234-1234-123456789abc\n" \
	"account.access_key = abcdef\n"

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

static Connection c;

static void test_defaults(void)
{
	CHECK(LoadFromString(&c, ACCOUNT));
	CHECK(strcmp(c.gateway_server.addr, "192.243.44.63") == 0);
	CHECK(c.gateway_server.port == 5999);
	CHECK(c.bot.command_prefix == '$');
	CHECK(c.bot.admin_name[0] == '\0');
	CHECK(c.bank.enabled == false);
	CHECK(c.bank.max_delivery_distance == 100);
}

static void test_all_options(void)
{
	CHECK(LoadFromString(&c,
		ACCOUNT
		"# comment\n"
		"; comment\n"
		"// comment\n"
		"\n"
		"   server.addr = 10.0.0.1   \r\n"
		"server.port = 6000\n"
		"client.version_patch = 400\n"
		"data.path = /tmp/lmbot/\n"
		"admin.name = Boss\n"
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
		"cargo_ship.reserve_gold = 10M\n"
	));
	
	CHECK(strcmp(c.gateway_server.addr, "10.0.0.1") == 0);
	CHECK(c.gateway_server.port == 6000);
	CHECK(c.app.version_patch == 400);
	CHECK(strcmp(c.bot.data_path, "/tmp/lmbot/") == 0);
	CHECK(strcmp(c.bot.admin_name, "Boss") == 0);
	CHECK(c.bot.command_prefix == '!');
	CHECK(c.bot.command_input == COMMAND_CHANNEL_WORLD);
	CHECK(c.bot.command_output == COMMAND_CHANNEL_GUILD);
	CHECK(c.auth.igg_id == 1234567890);
	CHECK(c.auth.session_len == 6);
	
	CHECK(c.bank.enabled);
	CHECK(c.bank.send_food);
	CHECK(!c.bank.send_rock);
	CHECK(c.bank.send_gold);
	CHECK(c.bank.reserve.food == 20000000);
	CHECK(c.bank.reserve.ore == 1500000);
	CHECK(c.bank.max_delivery_distance == 250);
	CHECK(c.bank.use_bag_rss);
	
	CHECK(c.alliance.auto_help);
	
	// shield_always_on used to overwrite protection.enabled
	CHECK(c.protection.enabled);
	CHECK(c.protection.shield_always_on);
	CHECK(c.protection.recall_on_incoming_attack);
	CHECK(c.protection.shield_priority_count == 3);
	CHECK(c.protection.shield_priority[0] == SHIELD_8H);
	CHECK(c.protection.shield_priority[1] == SHIELD_1D);
	CHECK(c.protection.shield_priority[2] == SHIELD_14D);
	
	CHECK(c.market.settings.auto_trade);
	CHECK(c.market.reserve.gold == 10000000);
}

static void test_shield_always_on_does_not_touch_enabled(void)
{
	CHECK(LoadFromString(&c, ACCOUNT
		"protection.enabled = false\n"
		"protection.shield_always_on = true\n"));
	CHECK(!c.protection.enabled);
	CHECK(c.protection.shield_always_on);
}

static void test_invalid_values(void)
{
	CHECK(!LoadFromString(&c, ACCOUNT "bank.enabled = maybe\n"));
	CHECK(!LoadFromString(&c, ACCOUNT "server.port = 70000\n"));
	CHECK(!LoadFromString(&c, ACCOUNT "command.input = CHAT\n"));
	CHECK(!LoadFromString(&c, ACCOUNT "command.prefix = !!\n"));
	CHECK(!LoadFromString(&c, ACCOUNT "protection.shield_priority = SHIELD_2H\n"));
	CHECK(!LoadFromString(&c, ACCOUNT "admin.name = ThisNameIsWayTooLong\n"));
	CHECK(!LoadFromString(&c, ACCOUNT "server.addr = 255.255.255.255.255\n"));
	CHECK(!LoadFromString(&c, ACCOUNT "bank.reserve_food = 10B\n"));
	CHECK(!LoadFromString(&c, ACCOUNT "this line has no equals sign\n"));
	CHECK(!LoadFromString(&c, ACCOUNT
		"protection.shield_priority = SHIELD_4H,SHIELD_4H,SHIELD_4H,SHIELD_4H,"
		"SHIELD_4H,SHIELD_4H,SHIELD_4H,SHIELD_4H,SHIELD_4H\n"));
}

static void test_reconnect_options(void)
{
	CHECK(LoadFromString(&c, ACCOUNT));
	CHECK(c.reconnect.enabled);
	CHECK(c.reconnect.delay == 5);
	CHECK(c.reconnect.max_delay == 300);
	CHECK(c.reconnect.max_attempts == 0);
	CHECK(c.reconnect.kicked_delay == 600);
	CHECK(c.reconnect.timeout == 90);
	
	CHECK(LoadFromString(&c, ACCOUNT
		"reconnect.enabled = false\n"
		"reconnect.delay = 2\n"
		"reconnect.max_delay = 60\n"
		"reconnect.max_attempts = 10\n"
		"reconnect.kicked_delay = 1200\n"
		"reconnect.timeout = 0\n"));
	CHECK(!c.reconnect.enabled);
	CHECK(c.reconnect.delay == 2);
	CHECK(c.reconnect.max_delay == 60);
	CHECK(c.reconnect.max_attempts == 10);
	CHECK(c.reconnect.kicked_delay == 1200);
	CHECK(c.reconnect.timeout == 0);
	
	CHECK(!LoadFromString(&c, ACCOUNT "reconnect.delay = 0\n"));
	CHECK(!LoadFromString(&c, ACCOUNT "reconnect.timeout = 10\n"));
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

static void test_unknown_key_is_warning(void)
{
	CHECK(LoadFromString(&c, ACCOUNT "some.future_option = 1\n"));
}

static void test_missing_account(void)
{
	CHECK(!LoadFromString(&c, "server.port = 5999\n"));
	CHECK(!LoadFromString(&c,
		"account.igg_id = 1234567890\n"
		"account.device_uuid = 12345678-1234-1234-1234-123456789abc\n"
		"account.access_key = YOUR_ACCESS_KEY_HERE\n"));
}

int main(void)
{
	test_parse_number();
	test_defaults();
	test_all_options();
	test_shield_always_on_does_not_touch_enabled();
	test_invalid_values();
	test_reconnect_options();
	test_reconnect_delay();
	test_unknown_key_is_warning();
	test_missing_account();
	
	if (failures) {
		printf("%d check(s) failed\n", failures);
		return 1;
	}
	
	printf("All config tests passed\n");
	return 0;
}
