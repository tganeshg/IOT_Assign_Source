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
| `pirVacancySec` | **PIR only** (`HD_1`, `HD_2`): in **Auto** mode, **minimum seconds between alternate ON/OFF toggles** (debounce between motion **edges**). The INI key name is historical; it does **not** mean “room vacant delay” anymore. Omit to use default **`PIR_TOGGLE_DEBOUNCE_SEC_DEFAULT`** (**2** s in `general.h`). If the key is **present**, **`0`** means no debounce (every new **0→1** coil transition may toggle). |

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

**LVGL** is driven via **`uiProcess()`** often in the loop (after init, around Modbus/DB/MQTT, and at the end of each iteration) so timers and input stay responsive.

**Auto mode + PIR:** loads follow an **alternate-detection** policy (not raw “PIR high = on”). On each **new motion detection**, the main process treats a **0→1** transition on the PIR read coil (after a throttled Modbus read) as one **toggle** step: **1st** detection → loads **ON**, **2nd** → **OFF**, **3rd** → ON, and so on. A **minimum time between toggles** is taken from **`pirVacancySec`** in the INI (seconds → ms); it suppresses false double-toggles from noisy retriggers while someone is still moving. **Manual** mode drives loads from the UI/MQTT only.

---

## 5) LVGL UI and touch

- **Display**: Custom framebuffer flush in `lvgl_port_linux.c` (not `lv_linux_fbdev`), with a capped partial buffer for small-RAM targets.
- **Rooms**: **Room 1** / **Room 2** tabs at the top; **Room 1** is the **default** visible panel on startup.
- **Per-room card**:
  - **Mode**: `lv_switch` for **Auto** / **Manual** (larger than stock dimensions + extended click area). A fixed-width **Auto/Manual** text label beside the switch avoids layout shift when the label text changes.
  - **Motion**: **Yes/No** (raw PIR coil) and **Person In** / **Person Out** on the same row. In **Auto**, “Person” tracks **alternate** occupancy (`pirToggleLoadsOn`); in **Manual**, it follows raw PIR for that column.
  - **Loads**: large ON/OFF bars for light/fan (room 1) or light/AC (room 2); room 2 mode row can show **Fridge** power when `RM_2` is present.
- **Policy**: Both rooms start in **Auto** mode in firmware unless changed by UI/MQTT.
- **`uiProcess()`** calls **`lv_timer_handler()`** multiple times per invocation so a busy Modbus/MQTT loop does not starve LVGL.
- **Touch**:
  - If **`LV_USE_EVDEV`** is **1** in LVGL’s `lv_conf.h` (typical Yocto/SDK build), pointer input uses **`lv_evdev_create()`** on `touchDev`, same idea as the reference `lv_linux_init_input_pointer()` pattern.
  - If **evdev is disabled**, the port uses a manual **`read()`** loop on the evdev node with **`EVIOCGABS`** calibration and recognizes **`BTN_TOUCH`**, **`ABS_MT_TRACKING_ID`**, pressure axes, and related keys where present (helps light taps).
- Input tuning: **1 ms** indev timer, **scroll limit** / **long-press** set in `indev_apply_responsive_touch()`; the main **root** flex container is **not** scrollable so vertical drags are less likely to steal taps.
- Widget actions use **`lv_obj_add_event_cb`** (mode switch, room tabs, loads).

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
| `smarthome/state/room1/pir` | `0` / `1` — **raw** PIR coil (same as **Motion** on the UI) |
| `smarthome/state/room2/pir` | `0` / `1` — **raw** PIR coil |
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
