#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "connection.h"
#include "log.h"
#include "net_rw.h"
#include "packet_enum.h"


#include "map_point.h"
#include "items.h"

#include "config.h"

#include "version.h"

#include <sys/epoll.h>


#include "dispatcher.h"

#include "utility.h"

#include "bot/tick.h"

#include "packet_map.h"


#include "bot/bank.h"

#include "reconnect.h"
#include <signal.h>

#define BUFFER_SIZE 4096

int epoll_fd = 0;

void ProcessConnection(Connection *c)
{
	PacketStream *s = &c->stream;
	
	while (s->read_pos - s->parse_pos >= 4) {
		s->packet_size = read_u16(s->buffer + s->parse_pos);
		s->packet_type = read_u16(s->buffer + s->parse_pos + 2);
		
		if (s->packet_size < 4 || s->packet_size > BUFFER_SIZE) {
			break;
		}
			
		if (s->read_pos - s->parse_pos < s->packet_size) {
			break;
		}
		
		DispatchPacket(c, s->packet_type, s->buffer + s->parse_pos + 4);
		
		/*
		// Debugging purpose only 
		LOGI("PACKET TYPE: %s (0x%X), size=%u\n", 
			get_packet_name(s->packet_type),
			s->packet_type,
			s->packet_size
		);
		*/
		
		/*
		if (s->packet_type == _MSG_RESP_TD_INFO || s->packet_type == _MSG_RESP_TD_TRIGGERINFO) {
			DumpData(get_packet_name(s->packet_type), s->buffer + s->parse_pos + 4, s->packet_size + 4);
		}
		
		
		if (s->packet_type == _MSG_MAGIC_GATE_DATA) {
			DumpData(get_packet_name(s->packet_type), s->buffer + s->parse_pos + 4, s->packet_size + 4);
		}
		*/
		
		
		/*
		// dump_data(get_packet_name(s->packet_type), "", s->buffer + s->parse_pos + 4, s->packet_size + 4);
		*/
		
		s->parse_pos += s->packet_size;
	}
	
	if (s->parse_pos > 0) {
		memmove(s->buffer, s->buffer + s->parse_pos, s->read_pos - s->parse_pos);
		s->read_pos -= s->parse_pos;
		s->parse_pos = 0;
	}
}

void PrintUsage(void)
{
	printf(
		"Lords Mobile Bot\n"
		"\n"
		"Usage:\n"
		"  client <config_file>         Load and start the bot using the specified configuration file.\n"
		"  client --create-config, -c   Create a default configuration file.\n"
		"  client --help, -h            Display this help message.\n"
		"  client --version, -v         Display version information.\n"
		"\n"
		"Project:\n"
		"  https://github.com/halloweeks/lords-mobile-bot\n"
	);
}

void PrintVersion(void)
{
    printf(
        "Lords Mobile Bot v%s\n"
        "Build: %s %s\n"
        "Project: https://github.com/halloweeks/lords-mobile-bot\n",
        VERSION,
        __DATE__,
        __TIME__);
}

// Loaded program configuration (server, client version, reconnect, bot list).
static ProgramConfig program;

// Scratch space used to reload a bot's configuration before reconnecting.
static Connection scratch;

static void HandleStopSignal(int sig)
{
	(void)sig;
	RequestStop();
}

/* Copy the global program settings into a bot. */
static void ApplyProgramConfig(Connection *c)
{
	snprintf(c->bot.data_path, sizeof(c->bot.data_path), "%s", program.data_path);

	c->app.version_major = program.version_major;
	c->app.version_minor = program.version_minor;
	c->app.version_patch = program.version_patch;
	c->app.language_code = program.language_code;

	snprintf(c->gateway_server.addr, sizeof(c->gateway_server.addr), "%s", program.server_addr);
	c->gateway_server.port = program.server_port;

	c->reconnect = program.reconnect;
}

/* Close the socket and remove it from epoll. */
static void CloseSocket(Connection *c)
{
	if (c->sock < 0)
		return;

	epoll_ctl(epoll_fd, EPOLL_CTL_DEL, c->sock, NULL);
	close(c->sock);
	c->sock = -1;
}

/*
 * End the current session and decide what happens next:
 * retry with exponential backoff, wait longer after being kicked by another
 * device, or stop for good when retrying cannot help.
 */
static void EndSession(Connection *c, SessionResult reason)
{
	CloseSocket(c);

	c->end_reason = reason;

	const long long id = (long long)c->auth.igg_id;
	const ReconnectSettings *rs = &c->reconnect;

	if (reason == SESSION_FATAL) {
		LOGE("[%lld] Not reconnecting: fix the problem above and restart the bot\n", id);
		c->state = CONN_CLOSED;
		return;
	}

	if (!rs->enabled) {
		LOGW("[%lld] Session ended (%s); reconnect disabled\n", id, SessionResultName(reason));
		c->state = CONN_CLOSED;
		return;
	}

	time_t now = time(NULL);

	// A session that stayed up for a while counts as a success: reset the backoff.
	if (c->session_start != 0 && now - c->session_start >= 300)
		c->reconnect_attempt = 0;

	c->reconnect_attempt++;

	if (rs->max_attempts && c->reconnect_attempt > rs->max_attempts) {
		LOGE("[%lld] Giving up after %u failed reconnect attempts\n", id, rs->max_attempts);
		c->state = CONN_CLOSED;
		return;
	}

	uint32_t delay = (reason == SESSION_KICKED)
		? rs->kicked_delay
		: ReconnectDelay(rs, c->reconnect_attempt);

	LOGW("[%lld] Session ended (%s). Reconnecting in %u seconds (attempt %u)...\n",
		id, SessionResultName(reason), delay, c->reconnect_attempt);

	c->reconnect_time = now + delay;
	c->state = CONN_RECONNECTING;
}

/*
 * Start a fresh session: reload the bot's configuration so no stale game
 * state carries over (and config edits take effect), then connect to the
 * gateway server.
 */
static void StartSession(Connection *c, const char *config_path)
{
	memset(&scratch, 0, sizeof(scratch));

	if (!LoadConfig(&scratch, config_path)) {
		LOGE("Failed to reload bot configuration: %s\n", config_path);
		EndSession(c, SESSION_DISCONNECTED);
		return;
	}

	ApplyProgramConfig(&scratch);

	// Keep state that must survive a reconnect.
	scratch.reconnect_attempt = c->reconnect_attempt;
	scratch.end_reason        = c->end_reason;
	scratch.bank_data         = c->bank_data;

	// Admin may have been changed with $su
	if (c->auth.igg_id != 0)
		memcpy(scratch.bot.admin_name, c->bot.admin_name, sizeof(scratch.bot.admin_name));

	memcpy(c, &scratch, sizeof(*c));
	c->sock = -1;

	c->session_start = time(NULL);
	c->last_recv     = c->session_start;

	if (!ConnectServer(c, c->gateway_server.addr, c->gateway_server.port)) {
		LOGE("[%lld] Failed to connect gateway %s:%u\n", (long long)c->auth.igg_id,
			c->gateway_server.addr, c->gateway_server.port);
		c->sock = -1;
		EndSession(c, SESSION_DISCONNECTED);
		return;
	}

	if (!epoll_register(epoll_fd, c, EPOLLIN | EPOLLOUT)) {
		LOGE("Failed to register socket with epoll\n");
		EndSession(c, SESSION_DISCONNECTED);
		return;
	}

	c->state = CONN_CONNECTING_GATEWAY;
	LOGI("[%lld] Connecting gateway server %s:%u\n", (long long)c->auth.igg_id,
		c->gateway_server.addr, c->gateway_server.port);
}

/* Stop listening for "writable" events, which would otherwise fire continuously. */
static void WatchReadOnly(Connection *c)
{
	struct epoll_event ev = {0};

	ev.events = EPOLLIN;
	ev.data.ptr = c;

	if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, c->sock, &ev) == -1) {
		LOGE("Failed to remove EPOLLOUT\n");
	}
}

void ConnectionMaintain(Connection *c, const char *config_path)
{
	time_t now = time(NULL);

	// No data for too long: the connection (or the connect attempt) is dead.
	if (c->sock >= 0 && c->reconnect.timeout &&
	    now - c->last_recv >= (time_t)c->reconnect.timeout) {
		LOGW("[%lld] No data from server for %u seconds\n", (long long)c->auth.igg_id, c->reconnect.timeout);
		EndSession(c, SESSION_TIMEOUT);
		return;
	}

	switch (c->state) {
		case CONN_CONNECTED_GATEWAY:
			LOGI("Gateway connected\n");

			RequestGuestLogIn(c);

			c->state = CONN_WAIT_GATEWAY_LOGIN;

			WatchReadOnly(c);
			break;
		case CONN_CONNECTED_GAME:
			LOGI("Game server connected\n");
			RequestLogIn(c);
			RequestClientInitOver(c);
			// c->state = CONN_WAIT_GAME_LOGIN;
			c->state = CONN_CONNECTED;

			c->action_state = 1; // start schedule tasks

			WatchReadOnly(c);
			break;
		case CONN_GATEWAY_LOGIN_SUCCESS:
			// 1. Tear down gateway fd
			CloseSocket(c);

			// 2. Reset the stream — gateway leftovers are meaningless to game parser
			memset(&c->stream, 0, sizeof(c->stream));

			// 3. Open new socket to game server (this sets c->sock internally)
			if (!ConnectServer(c, c->game_server.addr, c->game_server.port)) {
				LOGE("Failed to start game server connection\n");
				c->sock = -1;
				EndSession(c, SESSION_DISCONNECTED);
				return;
			}

			LOGI("Game server: %s:%u\n", c->game_server.addr, c->game_server.port);

			c->state = CONN_CONNECTING_GAME;
			c->last_recv = now;

			// 4. Register the NEW fd with epoll, same Connection* as data.ptr
			if (!epoll_register(epoll_fd, c, EPOLLIN | EPOLLOUT)) {
				LOGE("Failed to register game socket with epoll\n");
				EndSession(c, SESSION_DISCONNECTED);
				return;
			}

			break;
		case CONN_GATEWAY_LOGIN_FAILED:
			// RecvLoginError() set end_reason (fatal, kicked or retry)
			EndSession(c, c->end_reason);
			break;
		case CONN_DISCONNECTED:
			EndSession(c, SESSION_DISCONNECTED);
			break;
		case CONN_RECONNECTING:
			if (now >= c->reconnect_time) {
				StartSession(c, config_path);
			}
			break;
		default:
			break; // connected / logging in / running / closed — nothing to do here
    }
}

// For web based GUI currently under construction
// #include "bot/http_server.h"

int main(int argc, const char *argv[]) {
	// Line-buffered output keeps log lines in order when redirected to a file.
	setvbuf(stdout, NULL, _IOLBF, 0);

	DispatcherInit();

	if (argc < 2) {
		PrintUsage();
		return 0;
	}

	if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
		PrintUsage();
		return 0;
	}

	if (strcmp(argv[1], "--create-config") == 0 || strcmp(argv[1], "-c") == 0) {
		mkdir("configs", 0775);

		if (!CreateDefaultProgramConfig("program.cfg")) {
			LOGE("Failed to create 'program.cfg'\n");
			return EXIT_FAILURE;
		}

		if (!CreateDefaultConfig("configs/bank123.cfg")) {
			LOGE("Failed to create 'configs/bank123.cfg'\n");
			return EXIT_FAILURE;
		}

		LOGI("Created 'program.cfg'\n");
		LOGI("Created 'configs/bank123.cfg'\n");

		return EXIT_SUCCESS;
	}

	if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
		PrintVersion();
		return 0;
	}

	if (argc != 2) {
        printf("Usage: %s <program.cfg>\n", argv[0]);
        return EXIT_FAILURE;
    }

	// Load main program configuration file
	if (!LoadProgramConfig(argv[1], &program)) {
		LOGE("Failed to load program configuration: %s\n", argv[1]);
		return EXIT_FAILURE;
	}

	LOGI("Found %u bot configuration\n", program.bot_count);

	size_t memory_used = program.bot_count * sizeof(Connection);

	Connection *client = calloc(program.bot_count, sizeof(Connection));

	if (client == NULL) {
		LOGE("Failed to allocate memory for %u bot configurations\n", program.bot_count);
		return 1;
	}

	// Display the total memory allocated for all bot connections.
	if (memory_used >= 1024 * 1024) {
		LOGI("Allocated %u bot configurations (%.2f MB)\n", program.bot_count, (double)memory_used / (1024 * 1024));
	} else {
		LOGI("Allocated %u bot configurations (%.2f KB)\n", program.bot_count, (double)memory_used / 1024);
	}

	time_t now = time(NULL);

	for (size_t i = 0; i < program.bot_count; i++) {
		// Validate every bot configuration up front so mistakes show at startup.
		if (!LoadConfig(&client[i], program.config_path[i])) {
			LOGE("Failed to load bot configuration: %s\n", program.config_path[i]);
			free(client);
			return EXIT_FAILURE;
		}

		ApplyProgramConfig(&client[i]);

		client[i].sock = -1;

		// Connect one bot per second instead of all at once.
		client[i].state = CONN_RECONNECTING;
		client[i].reconnect_time = now + (time_t)i;
	}

	LOGI("data path: '%s'\n", program.data_path);

	LOGI("Client version: %u.%u.%u\n", program.version_major, program.version_minor, program.version_patch);

	/* Create epoll */
	epoll_fd = epoll_create1(0);

	if (epoll_fd == -1) {
		LOGE("Failed to create epoll instance\n");
		free(client);
		return EXIT_FAILURE;
	}

	signal(SIGINT, HandleStopSignal);
	signal(SIGTERM, HandleStopSignal);

	struct epoll_event events[64];
	int exit_code = EXIT_SUCCESS;

	while (!StopRequested()) {
		int n = epoll_wait(epoll_fd, events, 64, 1000);

		if (n == -1) {
			if (errno == EINTR)
				continue;

			/* Unexpected error. */
			LOGE("epoll_wait() failed\n");
			exit_code = EXIT_FAILURE;
			break;
		}

		for (int i = 0; i < n; i++) {
			Connection *c = events[i].data.ptr;

			if (c->sock < 0)
				continue; // closed earlier in this batch

			if (events[i].events & (EPOLLERR | EPOLLHUP)) {
				int err = 0;
				socklen_t len = sizeof(err);
				getsockopt(c->sock, SOL_SOCKET, SO_ERROR, &err, &len);

				LOGW("[%lld] Socket error: %s\n", (long long)c->auth.igg_id,
					err ? strerror(err) : "connection closed");
				EndSession(c, SESSION_DISCONNECTED);
				continue;
			}

			if (events[i].events & EPOLLOUT) {
				int err = 0;
				socklen_t len = sizeof(err);

				if (getsockopt(c->sock,
					SOL_SOCKET,
					SO_ERROR,
					&err,
					&len) == -1 || err != 0) {

					LOGW("[%lld] Connection failed: %s\n", (long long)c->auth.igg_id, strerror(err));
					EndSession(c, SESSION_DISCONNECTED);
					continue;
				}

				switch (c->state) {
					case CONN_CONNECTING_GATEWAY:
						c->state = CONN_CONNECTED_GATEWAY;
						break;
					case CONN_CONNECTING_GAME:
						c->state = CONN_CONNECTED_GAME;
						break;
					default:
						break;
				}
			}


			/* Incoming packets */
			if (events[i].events & EPOLLIN) {
				ssize_t ret = recv(c->sock,
					c->stream.buffer + c->stream.read_pos,
					BUFFER_SIZE - c->stream.read_pos,
				0);

				if (ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
					continue;

				if (ret <= 0) {
					LOGW("[%lld] %s\n", (long long)c->auth.igg_id,
						ret == 0 ? "Server closed the connection" : strerror(errno));
					EndSession(c, SESSION_DISCONNECTED);
					continue;
				}

				c->stream.read_pos += ret;
				c->last_recv = time(NULL);

				ProcessConnection(c);
			}
		}

		bool any_running = false;

		for (size_t i = 0; i < program.bot_count; i++) {
			Connection *c = &client[i];

			ConnectionMaintain(c, program.config_path[i]);

			if (c->state == CONN_CONNECTED) {
				BotTick(c);
			}

			if (c->state != CONN_CLOSED)
				any_running = true;
		}

		if (!any_running) {
			LOGE("All bots have stopped\n");
			exit_code = EXIT_FAILURE;
			break;
		}
	}

	if (StopRequested())
		LOGI("Stopped\n");

	for (size_t i = 0; i < program.bot_count; i++)
		CloseSocket(&client[i]);

	close(epoll_fd);
	free(client);
	return exit_code;
}
