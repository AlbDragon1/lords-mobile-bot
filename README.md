# Lords Mobile Bot

> ⚠️ **Dev Branch** — actively under development. Configuration and features may change.

A C-based Lords Mobile bot with multi-bot support, `epoll` networking, modular features, and command-based automation.

## Features

- Multiple bot support
- Linux `epoll` networking
- O(1) packet dispatcher
- Resource sending / Bank
- Automatic Alliance Help
- Automatic Cargo Ship trading
- Configurable command prefix

---

## Commands

The command prefix is configurable:

```ini
command.prefix = $
```

Default commands:

```
$food <amount> [target name]
$stone <amount> [target name]
$wood <amount> [target name]
$ore <amount> [target name]
$gold <amount> [target name]

$bal       (admin)
$abort     (alias: $stop)
$status
$help
```

`$abort` cancels the current transfer. It can be used by the player who asked
for it, the player receiving it, or the admin.

The admin (`admin.name`) can use resource commands even when `bank.enabled = false`
or a resource is not allowed. Leave `admin.name` empty to disable admin rights.

### Resource Examples

Send 10M Food to the player who executed the command:

```
$food 10M
```

Send 10M Food to a specific player:

```
$food 10M Player XYZ
```

Everything after the amount is treated as the target player's name, including spaces.

```
$food 10M Some Player Name
```

If no target name is provided, the resources are sent to the player who executed the command.

---

## Configuration

### Alliance Help

```ini
# Automatically send Guild Help to guild members whenever available.
alliance.auto_help = true
```

### Bank / Resource Sending

```ini
# Bank
# Master switch for the banking system.
bank.enabled = true

# Resource types allowed for delivery.
bank.send_food = true
bank.send_rock = true
bank.send_wood = true
bank.send_ore  = true
bank.send_gold = true

# Resource reserve.
bank.reserve_food = 0
bank.reserve_rock = 0
bank.reserve_wood = 0
bank.reserve_ore = 0
bank.reserve_gold = 0

# Maximum map distance (tiles) for resource delivery.
bank.max_delivery_distance = 30
```

### Cargo Ship Trading

```ini
# Automatically completes Cargo Ship trades.
cargo_ship.auto_trade = true

# Resources the bot is allowed to spend.
cargo_ship.spend_food = true
cargo_ship.spend_rock = true
cargo_ship.spend_wood = true
cargo_ship.spend_ore  = true
cargo_ship.spend_gold = true

# Use resource items from the bag when required.
cargo_ship.use_bag_rss = false

# Resource reserve limits.
cargo_ship.reserve_food = 10M
cargo_ship.reserve_rock = 10M
cargo_ship.reserve_wood = 10M
cargo_ship.reserve_ore  = 10M
cargo_ship.reserve_gold = 10M

# Items the bot WILL buy.
cargo_ship.trade_for_archaic_tome      = false
cargo_ship.trade_for_bright_talent_orb = false
cargo_ship.trade_for_exp_elixir        = false
cargo_ship.trade_for_speed_up          = false
cargo_ship.trade_for_speed_up_research = true

# Items the bot WILL ignore.
cargo_ship.trade_for_speed_up_training = false
cargo_ship.trade_for_speed_up_merging  = false
cargo_ship.trade_for_anima             = false

# Resource trades.
cargo_ship.trade_for_food = true
cargo_ship.trade_for_rock = true
cargo_ship.trade_for_wood = true
cargo_ship.trade_for_ore  = true
cargo_ship.trade_for_gold = true
```

### Auto-Reconnect

Set in `program.cfg`; applies to every bot. Each bot reconnects on its own
schedule, so one bot failing does not affect the others.

```ini
reconnect.enabled = true
# First retry delay in seconds; doubles after each failure (5, 10, 20, ...).
reconnect.delay = 5
reconnect.max_delay = 300
# Give up after this many failures in a row (0 = never).
reconnect.max_attempts = 0
# Wait this long when the account is logged in from another device,
# so the bot doesn't keep kicking you off your phone.
reconnect.kicked_delay = 600
# Treat a connection as dead after this many seconds without data (0 = off, else >= 30).
reconnect.timeout = 90
```

- A session that stays connected for 5 minutes resets the backoff.
- An outdated client version or expired access key stops that bot instead of retrying.
- Each reconnect reloads the bot's config file, so edits take effect on the next reconnect.
- Press `Ctrl+C` to stop all bots cleanly.

### Config File Rules

- Booleans accept `true`/`false` (also `yes`/`no`, `on`/`off`, `1`/`0`).
- Amounts accept `K`, `M` and `B` suffixes, e.g. `20M` or `1.5m`.
- An invalid value stops the bot with an error naming the file and line.
- An unknown key prints a warning (check for typos) and is ignored.
- `account.igg_id`, `account.device_uuid` and `account.access_key` are required.
- `--create-config` never overwrites existing files.

---

## Running Tests

```bash
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

---

## Command Prefix

The command prefix can be changed:

```ini
command.prefix = b
```

Commands will then use `b` instead of `$`:

```
bfood 10M
bstone 5M
bwood 10M
bore 10M
bgold 1M

bbal
babort
bstatus
bhelp
```

---

## Architecture

The dev branch uses an event-driven architecture based on Linux `epoll`.

```
                      epoll
                        │
                    Event Loop
                        │
          ┌─────────────┼─────────────┐
          │              │              │
        Bot #1         Bot #2         Bot #N
          │              │              │
          └─────────────┼─────────────┘
                        │
                Packet Dispatcher
                        │
                Feature Modules
          ┌─────────────┼─────────────┐
        Login        Alliance        Bank
                                       │
                                 Cargo Ship
```

Packet handlers use an O(1) dispatcher table instead of a large packet switch.

---

## Development Status

### Networking

- [x] Multiple bot connections
- [x] Linux epoll
- [x] Non-blocking sockets
- [x] O(1) packet dispatcher
- [x] Modular architecture
- [x] Auto-reconnect with exponential backoff

### Automation

- [x] Resource sending
- [x] Alliance Help
- [x] Cargo Ship trading
- [x] Command system

> The dev branch is not considered stable yet. Expect changes as development continues.
