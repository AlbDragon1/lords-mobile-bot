#ifndef _TRANSFER_H_
#define _TRANSFER_H_

#include <stdint.h>

struct Connection;

void ResourceTransferTick(struct Connection *c);
void SendResourceMarch(struct Connection *c);
uint32_t CalculateTransferAmount(struct Connection *c);

#endif