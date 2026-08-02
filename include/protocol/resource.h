#ifndef RESOURCE_H
#define RESOURCE_H

#include <stdint.h>
#include <stdbool.h>

struct Connection;

typedef struct Resources {
	bool loaded;
	uint32_t stock[5];
	uint64_t bag[5];
	uint64_t production[5];
} Resources;

void RecvResources(struct Connection*, const uint8_t*);
void RecvRefreshResources(struct Connection*, const uint8_t*);

void RecvResourceHelpReportInfo(struct Connection*, const uint8_t*);


void ResourceTransferTick(struct Connection *c);

void SendResource(struct Connection*, uint32_t stock[5], uint16_t zoneId, uint8_t pointId);
void RequestAllyPoint(struct Connection*, const char *);

void RecvSHelp(struct Connection*, const uint8_t*);
void RecvHelp_Home(struct Connection*, const uint8_t*);


void RecvMarchData(struct Connection *c, const uint8_t *data);

#endif