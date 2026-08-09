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

$bal
$abort
$status
```

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
cargo_ship.trade_for_archaic_tomes     = false
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

## Build

Linux is currently the primary supported platform.

```bash
git clone <repository>
cd lords-mobile-bot

git checkout dev

./build.sh
```

Or with CMake:

```bash
mkdir build
cd build
cmake ..
make
```

---

## Development Status

### Networking

- [x] Multiple bot connections
- [x] Linux epoll
- [x] Non-blocking sockets
- [x] O(1) packet dispatcher
- [x] Modular architecture

### Automation

- [x] Resource sending
- [x] Alliance Help
- [x] Cargo Ship trading
- [x] Command system

> The dev branch is not considered stable yet. Expect changes as development continues.
