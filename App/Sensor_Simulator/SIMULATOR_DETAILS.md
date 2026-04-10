# Sensor Simulator - Operation Details

This document explains the complete behavior of the `SensorSimulator` application.

The **Main Process** (`mainProc`) connects to these simulators over Modbus TCP and drives the smart-home UI and MQTT. See [`../Main_Process/MAIN_PROCESS.md`](../Main_Process/MAIN_PROCESS.md) for integration, config, and topics.

---

## 1) Launch and Config

Run the simulator with:

```bash
./SensorSimulator -c <config_file>
```

Example:

```bash
./SensorSimulator -c config/config_HD_1.ini
```

If `-c` is not provided, simulator prints help and exits.

---

## 2) Config File Keys

The simulator reads key-value pairs from the provided INI file.

### Required keys

- `sensorID`
- `modbusPort`

### Required for non-PIR sensors (`sensorID` not `1`/`4`)

- `minPower`
- `maxPower`

### Conditionally required key

- `ConfigGPIO` (also accepted: `ControlGPIO`, `configGPIO`, `controlGPIO`)
  - Required for control sensors and PIR sensors.

### Optional keys

- `debug` (`0/1`, `true/false`, `yes/no`)
- `modbusDebug` (`0/1`, `true/false`, `yes/no`)

---

## 3) Sensor ID Mapping

- `1 = HD_1` (Human Detector room 1)
- `2 = LMC_1` (Light Monitoring and Control room 1)
- `3 = FMC_1` (Fan Monitoring and Control room 1)
- `4 = HD_2` (Human Detector room 2)
- `5 = LMC_2` (Light Monitoring and Control room 2)
- `6 = AMC_2` (Air Conditioner Monitoring and Control room 2)
- `7 = RM_2` (Refrigerator Monitoring room 2)

---

## 4) Modbus Address Map

- Holding Register (read power): **`3000`**
- Read Coil (status): **`1000`**
- Write Coil (control command): **`5000`**

Common Modbus functions used by main process:

- Read Holding Registers: `0x03` (address `3000`)
- Read Coils: `0x01` (address `1000`)
- Write Single Coil: `0x05` (address `5000`, for control sensors only)

---

## 5) Sensor Behavior by Type

### A) PIR Sensors (`sensorID 1, 4`)

- `ConfigGPIO` is configured as **input**.
- On each Modbus request, simulator reads GPIO value and updates coil `1000`:
  - GPIO HIGH -> coil `1`
  - GPIO LOW -> coil `0`
- Main process polls coil `1000` on an interval configured per device in `config_MP.ini` (`pirPollMs`, typically **100–5000** ms; example configs often use **500** ms).
- Write coil is not allowed for PIR mode (illegal function response).
- No power value is exposed for PIR mode (coil-only behavior).

### B) Control and Monitoring Sensors (`sensorID 2, 3, 5, 6`)

- `ConfigGPIO` is configured as **output** (room1: light/fan, room2: light/AC).
- Main process writes coil `5000`:
  - `1` -> GPIO HIGH
  - `0` -> GPIO LOW
- Coil `1000` mirrors current control state for readback.
- Power is also available through holding register `3000`.

### C) Monitoring-only Sensor (`sensorID 7`, RM_2 / refrigerator)

- No control feature.
- No coil feature exposed.
- Only power monitoring via holding register `3000`.
- The main process maps this to **Room 2** UI (fridge power line on the mode row). There is no separate Room 1 fridge simulator in the default layout.

---

## 6) Power Simulation

- For non-PIR sensors, power values are simulated between `minPower` and `maxPower`.
- Simulator updates and serves the value via holding register `3000`.
- PIR sensors (`1`, `4`) skip power simulation and expose only PIR coil status.

---

## 7) Internal Runtime Flow

1. Parse `-c <config_file>`
2. Load and validate config
3. Configure GPIO based on sensor type (input or output)
4. Start Modbus TCP server on `modbusPort`
5. Accept main process connection
6. Loop:
   - simulate power
   - handle Modbus request
   - update/read coils as per sensor mode
   - respond to main process

---

## 8) Example Config Snippet

```ini
[SensorSimulator]
sensorID = 1
modbusPort = 502
ConfigGPIO = 5
debug = 1
modbusDebug = 0
```

---

## 9) Real PIR sensor and LEDs (hardware)

The same `SensorSimulator` binary can drive **real GPIO** on Linux (e.g. Raspberry Pi / embedded board). **`mainProc` does not need changes** for GPIO: it still uses Modbus TCP; only the simulator’s **physical layer** changes from “simulated values” to **PIR input** and **LED outputs**.

### PIR (motion) — `sensorID` **1** or **4** (`HD_1`, `HD_2`)

- `ConfigGPIO` selects a **GPIO used as input** via `/sys/class/gpio` (use the **pin number your kernel expects**, often **BCM** on Raspberry Pi—check your board’s GPIO map).
- On each Modbus read of coil **1000**, the simulator samples the pin: **HIGH → coil 1**, **LOW → coil 0** (motion / no motion depends on your PIR module’s idle output level—some are active-high on motion).
- Wire the PIR **data** pin to that GPIO (and **GND** / **3.3 V** or **5 V** per module datasheet). If the sensor is **5 V** output and the SoC is **3.3 V** input, use a **level shifter** or a compatible divider—do not exceed the CPU’s `Vih` max.
- Optional: enable internal pull-up/pull-down in software or add an external resistor if the line floats.

### LEDs (stand-in for Light / Fan / AC) — `sensorID` **2, 3, 5, 6**

- For these control sensors, `ConfigGPIO` is an **output**: **main process** writes Modbus coil **5000** (`1` = ON, `0` = OFF); the simulator drives the pin **HIGH/LOW** accordingly (same as driving an LED or relay).
- Use a **current-limiting resistor** in series with each LED (typ. a few hundred Ω at 3.3 V, value depends on LED colour and desired brightness). For **relays/mains loads**, drive a transistor or relay module instead of wiring the LED directly if current exceeds GPIO spec.

### Summary

| Role        | sensorID | GPIO direction | Modbus (simulator) |
|------------|----------|----------------|---------------------|
| PIR room 1 | 1        | **in**         | Read coil **1000**  |
| PIR room 2 | 4        | **in**         | Read coil **1000**  |
| Light R1   | 2        | **out**        | Write coil **5000** |
| Fan R1     | 3        | **out**        | Write coil **5000** |
| Light R2   | 5        | **out**        | Write coil **5000** |
| AC R2      | 6        | **out**        | Write coil **5000** |

Run **one** `SensorSimulator` process per `sensorID` with its own `modbusPort` and `ConfigGPIO`, matching `config_MP.ini` on the main process (same as pure software simulation).

