#ifndef CONFIG_H
#define CONFIG_H

#include "connection.h"
#include "items.h"

bool LoadConfig(Connection *c, const char *filename);

/* Parse a number with an optional K/M/B suffix, e.g. "1.5M" -> 1500000. */
uint64_t parse_number_u64(const char *str);

#endif
