#ifndef COMMAND_PARSER
#define COMMAND_PARSER

#include <stdint.h>
#include "connection.h"

void command_handler(Connection*, const char*, const char*);

void BalanceCommandHandler(
	Connection *c,
	const char *player_name,
	const char *message
);

void AbortCommandHandler(
	Connection *c,
	const char *player_name,
	const char *message
);

void StatusCommandHandler(
	Connection *c,
	const char *player_name,
	const char *message
);

#endif