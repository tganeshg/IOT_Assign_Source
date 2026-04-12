# Smart home web dashboard

Flask + Flask-SocketIO + MQTT client: live dashboard and power history aligned with `App/Main_Process` topics (`sensor/data`, `smarthome/state/*`, `smarthome/cmd/*`).

## Setup

```bash
cd Web
python3 -m venv .venv
source .venv/bin/activate   # Windows: .venv\Scripts\activate
pip install -r requirements.txt
cp config.ini.example config.ini
# Edit config.ini: MQTT broker, DB path, web host/port
```

## Run

```bash
source .venv/bin/activate
python app.py
```

Open `http://<host>:<port>/` (defaults from `[WEB]` in `config.ini`).

## Configuration (`config.ini`)

| Section    | Role |
|-----------|------|
| `[MQTT]`  | Broker address, port, optional credentials |
| `[DATABASE]` | SQLite file for logged `sensor/data` rows |
| `[WEB]`   | Bind `host` and `port` |
| `[PERSON]` | Debounce for Person In/Out mirroring main process Auto behaviour |

## HTTP API (summary)

| Route | Method | Purpose |
|-------|--------|---------|
| `/` | GET | Dashboard + History UI |
| `/api/state` | GET | JSON mirror of rooms, power (W), fridge |
| `/api/command` | POST | Publish load/mode commands to MQTT |
| `/api/history_series` | GET | Time series JSON for Plotly chart |
| `/api/current_power` | GET | Latest row per sensor from local DB |
| `/history/bar_graph`, `/history/line_graph` | GET | Plotly HTML fragments |

## MQTT topics (same names as main process)

**Subscribe (ingest):**

- `sensor/data` — power JSON from main process
- `smarthome/state/room1/mode`, `smarthome/state/room2/mode`
- `smarthome/state/room1/pir`, `smarthome/state/room2/pir`
- `smarthome/state/room1/light`, `smarthome/state/room1/fan`
- `smarthome/state/room2/light`, `smarthome/state/room2/ac`

**Publish (commands from UI):**

- `smarthome/cmd/room1/mode`, `smarthome/cmd/room2/mode`
- `smarthome/cmd/room1/light`, `smarthome/cmd/room1/fan`
- `smarthome/cmd/room2/light`, `smarthome/cmd/room2/ac`

Global topics `smarthome/cmd/mode` and `smarthome/state/mode` exist in the embedded app; this web UI uses the per-room topics above.

## Live updates

The UI polls `/api/state` periodically for live power and status. Socket.IO uses Engine.IO **long-polling** only (reliable with Werkzeug’s dev server). Ensure the MQTT broker matches `config.ini` and main process `config_MP.ini` so `sensor/data` reaches this service.

## Deploying to a cloud server

These steps assume a Linux VM (e.g. Ubuntu 22.04/24.04). The embedded main process (Modbus, LVGL) usually stays on the device; the cloud instance runs **only this Flask app**, which talks to MQTT and stores history in SQLite on the server.

### 1. Server preparation

- Install base packages: `sudo apt update && sudo apt install -y python3 python3-venv python3-pip git`
- In the cloud **security group / firewall**, allow **SSH (22)** and **HTTP/HTTPS (80/443)** as needed. Avoid exposing the MQTT broker publicly unless you use TLS, authentication, and a clear reason to do so.

### 2. Application install

```bash
git clone <your-repo-url> IOT_Assign_Source
cd IOT_Assign_Source/Web
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
cp config.ini.example config.ini
# Edit config.ini: [MQTT] broker, [WEB] host/port, [DATABASE] path
```

### 3. MQTT connectivity (critical)

The **main process** must publish to the same broker this app subscribes to. If the broker lives only on your **home LAN**, a cloud VM cannot reach it unless you add one of: **VPN** to home, **SSH tunnel**, **MQTT bridge**, or a **broker in the cloud** that both the device and the server use. For any broker reachable from the internet, prefer **TLS** and **username/password**.

### 4. Configuration on the server

- **`[MQTT]`** — host and port the **cloud server can reach** (not `127.0.0.1` unless Mosquitto runs on that same VM).
- **`[WEB]`** — for a direct test, `host = 0.0.0.0` and `port = 5000` with the cloud firewall opening `5000` is enough; for production, bind to `127.0.0.1` and put **Nginx** in front (below).

### 5. Smoke test

```bash
cd IOT_Assign_Source/Web && source .venv/bin/activate
python app.py
```

Open `http://<server-public-ip>:<port>/` and confirm the dashboard and live data.

### 6. Run as a systemd service (survives logout/reboot)

Adjust paths to match your install. Example unit at `/etc/systemd/system/iot-web.service`:

```ini
[Unit]
Description=IOT Smart Home Web
After=network.target

[Service]
User=www-data
WorkingDirectory=/path/to/IOT_Assign_Source/Web
Environment=PATH=/path/to/IOT_Assign_Source/Web/.venv/bin
ExecStart=/path/to/IOT_Assign_Source/Web/.venv/bin/python app.py
Restart=always

[Install]
WantedBy=multi-user.target
```

Then:

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now iot-web.service
sudo journalctl -u iot-web.service -f
```

### 7. HTTPS reverse proxy (recommended)

Install **Nginx** (and **Certbot** for Let’s Encrypt if you have a domain). Terminate TLS on Nginx and **proxy** to `http://127.0.0.1:5000`. The app uses Engine.IO **long-polling**; if you later enable WebSockets, configure `Upgrade`/`Connection` headers and increase `proxy_read_timeout` if clients disconnect. For polling-only traffic, standard proxy settings often work; raise timeouts if you see flaky Socket.IO.

### 8. Hardening (short checklist)

- Do not commit real `config.ini` (already gitignored); use secrets or environment variables if you extend the app.
- Prefer **SSH keys**, disable password SSH if appropriate, and keep the OS updated.
- Restrict who can reach the dashboard (VPN, firewall allowlist, or Nginx rules) if it is not meant to be public.

### 9. What moves where

| Component | Typical location |
|-----------|------------------|
| Main process (Modbus, UI on device) | Edge device |
| This Flask app | Cloud VM (or same LAN) |
| MQTT broker | Reachable by **both** device and server |
| `sensor_data_web.db` | On the server running Flask |
