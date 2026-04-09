# Sensor Simulator - Operation Details

This document explains the complete behavior of the `SensorSimulator` application.

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
- Main process can poll coil `1000` every 500 ms for quick PIR status.
- Write coil is not allowed for PIR mode (illegal function response).
- No power value is exposed for PIR mode (coil-only behavior).

### B) Control and Monitoring Sensors (`sensorID 2, 3, 5, 6`)

- `ConfigGPIO` is configured as **output** (room1: light/fan, room2: light/AC).
- Main process writes coil `5000`:
  - `1` -> GPIO HIGH
  - `0` -> GPIO LOW
- Coil `1000` mirrors current control state for readback.
- Power is also available through holding register `3000`.

### C) Monitoring-only Sensor (`sensorID 7`)

- No control feature.
- No coil feature exposed.
- Only power monitoring via holding register `3000`.

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

