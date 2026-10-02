#ifndef CONFIG_H
#define CONFIG_H

#include "items.h"
#include <stdbool.h>
#include <stdint.h>

#include "reconnect.h"

struct Connection;

typedef struct {
	char server_addr[16];
	uint16_t server_port;
	
	uint8_t version_major;
	uint8_t version_minor;
	uint16_t version_patch;
	
	uint8_t language_code;
	char data_path[256];
	
	uint32_t bot_count;
	
	char config_path[256][256];
	
	ReconnectSettings reconnect;
} ProgramConfig;

bool CreateDefaultConfig(const char *filename);
bool CreateDefaultProgramConfig(const char *filename);

bool LoadConfig(struct Connection *c, const char *filename);
bool LoadProgramConfig(const char *filepath, ProgramConfig *program);

/* Parse a number with an optional K/M/B suffix, e.g. "1.5M" -> 1500000. */
uint64_t parse_number_u64(const char *str);

#endif