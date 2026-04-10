# Main Process (`mainProc`) — Operation Details

The main application connects to all sensor simulators over **Modbus TCP**, stores readings in **SQLite**, publishes data over **MQTT**, and (when built with `ENABLE_LVGL`) runs a **touch UI** on Linux framebuffer.

For simulator behavior and Modbus maps, see [`../Sensor_Simulator/SIMULATOR_DETAILS.md`](../Sensor_Simulator/SIMULATOR_DETAILS.md). **Real PIR + LEDs** are wired at the simulator side (GPIO); main process behavior is unchanged—see **§9** in that file.

---

## 1) Build

- **Makefile** in this directory: set `RASPI=1` for cross-compile paths, or use host toolchain with LVGL available via `pkg-config` (`lvgl`).
- **LVGL** is enabled with `-DENABLE_LVGL` (default in the Makefile). To build without UI, that define would need to be removed and UI call sites stubbed — the shipped tree expects LVGL on the target.

---

## 2) Launch

```bash
./mainProc -n <sensor_count>
```

- **`-n`** — number of sensor simulators **1–7**, matching the order in `general.h` / config sections: `HD_1`, `LMC_1`, `FMC_1`, `HD_2`, `LMC_2`, `AMC_2`, `RM_2`.
- **`-d`** — debug log.
- **`-m`** — Modbus library debug.

Default config path: **`/home/root/config/config_MP.ini`** (`CONFIG_FILE` in `general.h`).

---

## 3) Config file (`config_MP.ini`)

### Sensor sections

Each section **`[HD_1]` … `[RM_2]`** (subset allowed if `-n` &lt; 7) supports:

| Key            | Meaning |
|----------------|---------|
| `sensorIP`     | Modbus TCP host |
| `sensorPort`   | Modbus TCP port |
| `readInterval` | Used in DB / publish timing context |
| `pirPollMs`    | **PIR only** (`HD_1`, `HD_2`): poll interval **100–5000** ms |

### UI section `[ui]`

| Key        | Meaning |
|------------|---------|
| `fbdev`    | Framebuffer device (e.g. `/dev/fb0`) |
| `touchDev` | Evdev node (e.g. `/dev/input/event0`) |

### MQTT section `[mqtt]`

| Key | Meaning |
|-------------------------|---------|
| `mqttIP` / `mqttPort`   | Broker |
| `mqttUsername` / `mqttPassword` | Optional |
| `publishInterval`       | Sensor data publish period (seconds) |
| `statePublishInterval`  | Room/mode state topic period (seconds) |

---

## 4) State machine (summary)

Rough loop:

1. **INIT** — Load config, open DB, MQTT client, connect broker, start Mosquitto loop thread.
2. **CONNECT_MQTT** — Wait until MQTT connected (UI pumped; no long sleeps).
3. **CONNECT_MODBUS** — Per device: connect, read PIR or power, apply auto/manual policy, write control coils, refresh UI state.
4. **INSERT_DB** — Insert non-PIR power rows.
5. **PUBLISH_MQTT** — Data and state topics when intervals elapse.
6. Repeat from **CONNECT_MODBUS** unless **ERROR**.

**LVGL** is driven via **`uiProcess()`** at the top of the loop and during Modbus / DB / MQTT work so touch stays responsive.

---

## 5) LVGL UI and touch

- **Display**: Custom framebuffer flush in `lvgl_port_linux.c` (not `lv_linux_fbdev`), with a capped partial buffer for small-RAM targets.
- **Touch**:
  - If **`LV_USE_EVDEV`** is **1** in LVGL’s `lv_conf.h` (typical Yocto/SDK build), pointer input uses **`lv_evdev_create()`** on `touchDev`, same idea as the reference `lv_linux_init_input_pointer()` pattern.
  - If **evdev is disabled**, the port falls back to a manual **`read()`** on the evdev node with calibration from `EVIOCGABS`.
- Widget actions use **`lv_obj_add_event_cb`** (e.g. mode switches, room tabs, loads) — equivalent to registering callbacks on controls in sample apps, not a separate global “touch” callback.

---

## 6) MQTT topics

Command (subscribe) and state (publish) topics include:

| Topic | Role |
|-------|------|
| `smarthome/cmd/mode` | Legacy: set both rooms’ mode (`AUTO` / `MANUAL`) |
| `smarthome/cmd/room1/mode` | Room 1 mode |
| `smarthome/cmd/room2/mode` | Room 2 mode |
| `smarthome/cmd/room1/light` | Room 1 light |
| `smarthome/cmd/room1/fan` | Room 1 fan |
| `smarthome/cmd/room2/light` | Room 2 light |
| `smarthome/cmd/room2/ac` | Room 2 AC |
| `smarthome/state/mode` | Aggregate: `AUTO`, `MANUAL`, or `MIXED` |
| `smarthome/state/room1/mode` | Room 1 mode |
| `smarthome/state/room2/mode` | Room 2 mode |
| `smarthome/state/room1/pir` | `0` / `1` |
| `smarthome/state/room2/pir` | `0` / `1` |
| `smarthome/state/room1/light` | `0` / `1` |
| `smarthome/state/room1/fan` | `0` / `1` |
| `smarthome/state/room2/light` | `0` / `1` |
| `smarthome/state/room2/ac` | `0` / `1` |

Sensor bulk topic: **`sensor/data`** (see publish path in code).

---

## 7) Modbus alignment with simulator

| Function | Address | Use |
|----------|---------|-----|
| Read holding | **3000** | Power (W) for non-PIR / monitoring |
| Read coil | **1000** | PIR or mirrored control state |
| Write coil | **5000** | Control outputs (light/fan/AC) |

Indices in the main process: `HD_1=0`, `LMC_1=1`, `FMC_1=2`, `HD_2=3`, `LMC_2=4`, `AMC_2=5`, `RM_2=6`.

---

## 8) Related scripts

`App/launch.sh` can start all simulators from `Sensor_Simulator/config` then `mainProc` — see comments in that script for directory layout on the device.
