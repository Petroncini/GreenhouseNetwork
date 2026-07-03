# Greenhouse Network

Application-layer protocol implementation for a smart greenhouse system.

**SSC0142 — Redes de Computadores (2026)**
Caio Petroncini · Natalie Isernhagen Coelho · Nicolas de Sousa Maia

---

## Requirements

- `g++` with C++17 support
- POSIX system (Linux / macOS)
- A GUI terminal emulator (gnome-terminal, xfce4-terminal, konsole, or xterm)

---

## Quick start

### 1. Compile

```bash
make
```

Only files that have changed since the last build are recompiled. To force a full rebuild:

```bash
make clean && make
```

### 2. Run the full network

```bash
chmod +x run_network.sh
./run_network.sh
```

This compiles everything (if needed), then opens a separate terminal window for each device:

| Window | Process |
|---|---|
| Manager | Central server |
| Sensor - Temp (ID 1) | Temperature sensor |
| Sensor - Soil (ID 2) | Soil moisture sensor |
| Sensor - CO2 (ID 3) | CO₂ sensor |
| Actuator - Heater (ID 11) | Heater |
| Actuator - Irrigation (ID 12) | Irrigation system |
| Actuator - Injector (ID 13) | CO₂ injector |
| Actuator - Cooler (ID 14) | Cooler |

Press **Enter** in the script terminal to shut everything down.

### 3. Run the controller (separate terminal)
Open a new terminal tab or window and run:

```bash
./controller
```

#### Controller commands

```
> 1 <type>                 Query the last sensor reading of a data type
> 2 <type>                 Query current min/max limits of the type
> 3 <type> <min|max> <val> Set the min or max limit of the type
> q                        Disconnect
```

`<type>`: `0` = temperature · `1` = humidity · `2` = CO₂

`<boundary>`: `0` = min · `1` = max

**Examples:**
```
> 1 0          Query current temperature
> 2 0          Read temperature limits (min and max)
> 3 0 1 30     Set temperature MAX to 30
> 3 0 0 15     Set temperature MIN to 15
> q            Exit
```

### 4. Stop all processes

Simply press **Enter** in the `run_network.sh` terminal.

Alternatively, from any other terminal:

```bash
chmod +x stop_network.sh
./stop_network.sh
```

---

## Running devices manually

If you need to start individual devices outside of the script:

```bash
./manager

./sensor <id> <type>
# e.g.: ./sensor 1 temp

./actuator <id> <type>
# e.g.: ./actuator 11 heater

./controller
```

**Device type values** (same for sensor and actuator):

| Value | Aliases | Meaning |
|---|---|---|
| `0` | `temp`, `heater`, `temperature` | Temperature / Heater |
| `1` | `soil`, `moisture`, `irrigation` | Soil moisture / Irrigation |
| `2` | `co2`, `injector` | CO₂ / Injector |
| `3` | `cooler` | Cooler |

> IDs must be unique within the same device class (sensors and actuators each have their own ID space).

> The manager must be started before any other component.
