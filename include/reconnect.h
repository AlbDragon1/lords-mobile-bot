#ifndef RECONNECT_H
#define RECONNECT_H

#include <stdint.h>
#include <stdbool.h>

/* Why a connection session ended. */
typedef enum {
	SESSION_GATEWAY_OK,    // Gateway login done, game server address received.
	SESSION_DISCONNECTED,  // Connection lost; retry with backoff.
	SESSION_TIMEOUT,       // Server stopped sending data; retry with backoff.
	SESSION_KICKED,        // Account logged in from another device.
	SESSION_FATAL,         // Retrying will not help (outdated client, expired key).
	SESSION_STOPPED        // User asked the bot to stop (Ctrl+C).
} SessionResult;

typedef struct {
	bool enabled;
	uint32_t delay;         // First retry delay in seconds.
	uint32_t max_delay;     // Upper bound for the exponential backoff.
	uint32_t max_attempts;  // Consecutive failed attempts before giving up (0 = unlimited).
	uint32_t kicked_delay;  // Wait after "logged in from another device".
	uint32_t timeout;       // Seconds without data before the connection is considered dead.
} ReconnectSettings;

const char *SessionResultName(SessionResult result);

/*
 * Delay before reconnect attempt number `attempt` (1-based):
 * delay * 2^(attempt-1), capped at max_delay.
 */
uint32_t ReconnectDelay(const ReconnectSettings *s, uint32_t attempt);

/* Stop flag, set from the SIGINT/SIGTERM handler. */
void RequestStop(void);
bool StopRequested(void);

/* Sleep for `seconds`, returning early (false) if a stop is requested. */
bool InterruptibleSleep(uint32_t seconds);

#endif
