#include "reconnect.h"

#include <signal.h>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <unistd.h>
#endif

static volatile sig_atomic_t stop_requested = 0;

const char *SessionResultName(SessionResult result)
{
	switch (result) {
		case SESSION_GATEWAY_OK:   return "gateway ok";
		case SESSION_DISCONNECTED: return "disconnected";
		case SESSION_TIMEOUT:      return "timed out";
		case SESSION_KICKED:       return "logged in from another device";
		case SESSION_FATAL:        return "fatal error";
		case SESSION_STOPPED:      return "stopped";
	}
	
	return "unknown";
}

uint32_t ReconnectDelay(const ReconnectSettings *s, uint32_t attempt)
{
	uint32_t delay = s->delay ? s->delay : 1;
	uint32_t max_delay = s->max_delay > delay ? s->max_delay : delay;
	
	for (uint32_t i = 1; i < attempt; i++) {
		if (delay >= max_delay / 2) {
			delay = max_delay;
			break;
		}
		delay *= 2;
	}
	
	return delay < max_delay ? delay : max_delay;
}

void RequestStop(void)
{
	stop_requested = 1;
}

bool StopRequested(void)
{
	return stop_requested != 0;
}

bool InterruptibleSleep(uint32_t seconds)
{
	// Sleep in small steps so Ctrl+C is handled promptly.
	for (uint32_t ms = 0; ms < seconds * 1000u; ms += 100) {
		if (StopRequested())
			return false;
#ifdef _WIN32
		Sleep(100);
#else
		usleep(100 * 1000);
#endif
	}
	
	return !StopRequested();
}
