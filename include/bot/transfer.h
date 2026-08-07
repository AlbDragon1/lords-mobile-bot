#ifndef _TRANSFER_H_
#define _TRANSFER_H_

#include <stdint.h>
#include <time.h>

typedef enum {
    TRANSFER_IDLE,
    TRANSFER_FIND_TARGET,
    TRANSFER_WAIT_TARGET,
    TRANSFER_SEND_MARCH,
    TRANSFER_WAIT_MARCH,
    TRANSFER_COMPLETE,
    TRANSFER_FAILED,
    
    TRANSFER_FAILED_TIMEOUT,
    TRANSFER_FAILED_TOO_FAR,
    TRANSFER_FAILED_OTHER_KINGDOM,
} TransferState;

typedef struct {
	char issued_name[13]; // Who initiated resource command?
    char target_name[13]; // Who will receive resource?
    
    uint8_t resource_type; // type 0,1,2,3,4
    uint32_t stock[5];
    
    time_t timeout;
    
    uint8_t max_marches;
    uint8_t cur_marches;
    
    uint32_t amount;
    uint32_t remaining;

    uint16_t zone_id;
    uint8_t point_id;

    TransferState state;
} ResourceTransfer;


struct Connection;

void ResourceTransferTick(struct Connection *c);
void SendResourceMarch(struct Connection *c);
uint32_t CalculateTransferAmount(struct Connection *c);

#endif