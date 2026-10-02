#ifndef COMMAND_PARSER
#define COMMAND_PARSER

#include <stdint.h>
#include "connection.h"

void command_handler(Connection*, const char*, const char*);

#endif
