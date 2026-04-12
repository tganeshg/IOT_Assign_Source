#!/usr/bin/env python3
"""
Smart home web UI: MQTT dashboard/control + power history (Flask + Socket.IO + SQLite).
Aligns with mainProc topics in App/Main_Process/include/general.h.
"""

from __future__ import annotations

import configparser
import json
import sqlite3
import threading
import time
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Tuple

import pandas as pd
import plotly.express as px
import paho.mqtt.client as mqtt
from flask import Flask, jsonify, render_template, request
from flask_socketio import SocketIO

# --- MQTT topic names (must match mainProc) ---
TOPIC_SENSOR_DATA = "sensor/data"
TOPIC_CMD_R1_MODE = "smarthome/cmd/room1/mode"
TOPIC_CMD_R2_MODE = "smarthome/cmd/room2/mode"
TOPIC_CMD_R1_LIGHT = "smarthome/cmd/room1/light"
TOPIC_CMD_R1_FAN = "smarthome/cmd/room1/fan"
TOPIC_CMD_R2_LIGHT = "smarthome/cmd/room2/light"
TOPIC_CMD_R2_AC = "smarthome/cmd/room2/ac"
TOPIC_STATE_R1_MODE = "smarthome/state/room1/mode"
TOPIC_STATE_R2_MODE = "smarthome/state/room2/mode"
TOPIC_STATE_R1_PIR = "smarthome/state/room1/pir"
TOPIC_STATE_R2_PIR = "smarthome/state/room2/pir"
TOPIC_STATE_R1_LIGHT = "smarthome/state/room1/light"
TOPIC_STATE_R1_FAN = "smarthome/state/room1/fan"
TOPIC_STATE_R2_LIGHT = "smarthome/state/room2/light"
TOPIC_STATE_R2_AC = "smarthome/state/room2/ac"

# Device_ID in DB / sensor/data JSON: 1..7 (see main.c insertDB idx+1)
SENSOR_LABELS = {
    1: "R1 PIR",
    2: "R1 Light",
    3: "R1 Fan",
    4: "R2 PIR",
    5: "R2 Light",
    6: "R2 AC",
    7: "R2 Fridge",
}


@dataclass
class RoomMirror:
    """Tracks published state + Person In/Out for Auto (edge + debounce, like mainProc)."""

    auto: bool = True
    pir: int = 0
    prev_pir: int = 0
    person: int = 0
    light: int = 0
    fan_or_dummy: int = 0  # R1 fan / R2 AC
    last_toggle_ms: int = 0


@dataclass
class AppState:
    r1: RoomMirror = field(default_factory=RoomMirror)
    r2: RoomMirror = field(default_factory=RoomMirror)
    fridge_power_w: int = 0
    """Latest W per sensorID from sensor/data (same as mainProc Modbus read)."""
    power_raw_w: Dict[int, int] = field(default_factory=dict)
    lock: threading.Lock = field(default_factory=threading.Lock)

    def to_dict(self) -> Dict[str, Any]:
        with self.lock:
            pw = self.power_raw_w
            return {
                "room1": {
                    "auto": self.r1.auto,
                    "pir": self.r1.pir,
                    "person": self.r1.person,
                    "light": self.r1.light,
                    "fan": self.r1.fan_or_dummy,
                },
                "room2": {
                    "auto": self.r2.auto,
                    "pir": self.r2.pir,
                    "person": self.r2.person,
                    "light": self.r2.light,
                    "ac": self.r2.fan_or_dummy,
                },
                "fridgePowerW": pw.get(7, self.fridge_power_w),
                "powerW": {
                    "r1Light": pw.get(2, 0),
                    "r1Fan": pw.get(3, 0),
                    "r2Light": pw.get(5, 0),
                    "r2Ac": pw.get(6, 0),
                    "fridge": pw.get(7, 0),
                },
                # Latest W per sensorID for History gauges (live, no extra DB read)
                "powerRawById": {str(k): int(v) for k, v in sorted(self.power_raw_w.items())},
            }


state = AppState()
debounce_sec: float = 2.0
mqtt_client: Optional[mqtt.Client] = None
app = Flask(__name__)
socketio = SocketIO(app, async_mode="threading", cors_allowed_origins="*")


def _emit(event: str, data: Any) -> None:
    """Socket.IO from the MQTT thread: needs app context + broadcast or clients often get nothing."""
    try:
        with app.app_context():
            socketio.emit(event, data, namespace="/", broadcast=True)
    except Exception:
        pass


config = configparser.ConfigParser()
config.read("config.ini")


def _cfg() -> None:
    global debounce_sec
    debounce_sec = float(config.get("PERSON", "debounce_sec", fallback="2"))


_cfg()

mqtt_broker = config["MQTT"]["broker"]
mqtt_port = int(config["MQTT"]["port"])
mqtt_username = config["MQTT"].get("username", fallback="").strip() or None
mqtt_password = config["MQTT"].get("password", fallback="").strip() or None
db_name = config["DATABASE"]["name"]
web_host = config["WEB"]["host"]
web_port = int(config["WEB"]["port"])


def init_db() -> None:
    conn = sqlite3.connect(db_name)
    cur = conn.cursor()
    cur.execute(
        """
        CREATE TABLE IF NOT EXISTS SensorData (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            sensorID INTEGER,
            power INTEGER,
            timestamp TEXT
        )
        """
    )
    conn.commit()
    conn.close()


def _apply_pir_room(room: RoomMirror, new_pir: int, now_ms: int) -> None:
    """Manual: person follows PIR. Auto: toggle person on 0->1 with debounce (matches mainProc)."""
    if not room.auto:
        room.pir = new_pir
        room.person = new_pir
        room.prev_pir = new_pir
        return

    if room.prev_pir == 0 and new_pir == 1:
        min_gap_ms = int(debounce_sec * 1000)
        if now_ms - room.last_toggle_ms >= min_gap_ms:
            room.person = 1 - room.person
            room.last_toggle_ms = now_ms
    room.prev_pir = new_pir
    room.pir = new_pir


def handle_state_payload(topic: str, payload: str) -> None:
    now_ms = int(time.time() * 1000)
    v = payload.strip()
    bit = 1 if v in ("1", "on", "true", "ON", "TRUE") else 0
    mode_auto = None
    if v.upper() == "AUTO":
        mode_auto = True
    elif v.upper() == "MANUAL":
        mode_auto = False

    with state.lock:
        if topic == TOPIC_STATE_R1_MODE and mode_auto is not None:
            state.r1.auto = mode_auto
            if mode_auto:
                state.r1.prev_pir = state.r1.pir
            else:
                state.r1.person = state.r1.pir
                state.r1.prev_pir = state.r1.pir
        elif topic == TOPIC_STATE_R2_MODE and mode_auto is not None:
            state.r2.auto = mode_auto
            if mode_auto:
                state.r2.prev_pir = state.r2.pir
            else:
                state.r2.person = state.r2.pir
                state.r2.prev_pir = state.r2.pir
        elif topic == TOPIC_STATE_R1_PIR:
            _apply_pir_room(state.r1, bit, now_ms)
        elif topic == TOPIC_STATE_R2_PIR:
            _apply_pir_room(state.r2, bit, now_ms)
        elif topic == TOPIC_STATE_R1_LIGHT:
            state.r1.light = bit
        elif topic == TOPIC_STATE_R1_FAN:
            state.r1.fan_or_dummy = bit
        elif topic == TOPIC_STATE_R2_LIGHT:
            state.r2.light = bit
        elif topic == TOPIC_STATE_R2_AC:
            state.r2.fan_or_dummy = bit

    _emit("state", state.to_dict())


def handle_sensor_data(payload: str) -> None:
    try:
        decoded = json.loads(payload)
    except json.JSONDecodeError:
        return
    if isinstance(decoded, dict):
        rows = [decoded]
    elif isinstance(decoded, list):
        rows = decoded
    else:
        return

    parsed: List[Tuple[int, int, str]] = []
    for entry in rows:
        if not isinstance(entry, dict):
            continue
        try:
            sid_raw = entry.get("sensorID", entry.get("sensor_id"))
            if sid_raw is None:
                continue
            sid = int(sid_raw)
            power = int(entry["power"])
            ts = str(entry.get("Timestamp") or entry.get("timestamp", ""))
        except (KeyError, TypeError, ValueError):
            continue
        parsed.append((sid, power, ts))

    with state.lock:
        for sid, power, _ts in parsed:
            state.power_raw_w[sid] = power
            if sid == 7:
                state.fridge_power_w = power

    conn = sqlite3.connect(db_name)
    cur = conn.cursor()
    for sid, power, ts in parsed:
        cur.execute(
            "INSERT INTO SensorData (sensorID, power, timestamp) VALUES (?, ?, ?)",
            (sid, power, ts),
        )
    conn.commit()
    conn.close()

    _emit("sensor_batch", rows)
    _emit("state", state.to_dict())


def on_mqtt_message(_client: mqtt.Client, _userdata: Any, msg: mqtt.MQTTMessage) -> None:
    topic = msg.topic
    payload = msg.payload.decode("utf-8", errors="replace")
    if topic == TOPIC_SENSOR_DATA:
        handle_sensor_data(payload)
        return
    if topic.startswith("smarthome/state/"):
        handle_state_payload(topic, payload)


def start_mqtt() -> None:
    global mqtt_client
    mqtt_client = mqtt.Client(
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
        client_id="iot_web_ui",
        protocol=mqtt.MQTTv311,
    )
    if mqtt_username and mqtt_password:
        mqtt_client.username_pw_set(mqtt_username, mqtt_password)
    mqtt_client.on_message = on_mqtt_message
    mqtt_client.connect(mqtt_broker, mqtt_port, keepalive=60)
    mqtt_client.subscribe(TOPIC_SENSOR_DATA)
    for t in (
        TOPIC_STATE_R1_MODE,
        TOPIC_STATE_R2_MODE,
        TOPIC_STATE_R1_PIR,
        TOPIC_STATE_R2_PIR,
        TOPIC_STATE_R1_LIGHT,
        TOPIC_STATE_R1_FAN,
        TOPIC_STATE_R2_LIGHT,
        TOPIC_STATE_R2_AC,
    ):
        mqtt_client.subscribe(t)
    mqtt_client.loop_start()


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/state")
def api_state():
    r = jsonify(state.to_dict())
    r.headers["Cache-Control"] = "no-store, no-cache, must-revalidate, max-age=0"
    r.headers["Pragma"] = "no-cache"
    return r


@app.route("/api/command", methods=["POST"])
def api_command():
    data = request.get_json(silent=True) or {}
    room = int(data.get("room", 0))
    cmd = (data.get("cmd") or "").lower()
    val = data.get("value")

    if mqtt_client is None:
        return jsonify({"ok": False, "error": "mqtt not ready"}), 503

    topic = None
    payload = None

    if cmd == "mode":
        payload = "AUTO" if val in (True, 1, "1", "AUTO", "auto") else "MANUAL"
        topic = TOPIC_CMD_R1_MODE if room == 1 else TOPIC_CMD_R2_MODE
    elif cmd == "light":
        payload = "1" if val in (True, 1, "1", "on") else "0"
        topic = TOPIC_CMD_R1_LIGHT if room == 1 else TOPIC_CMD_R2_LIGHT
    elif cmd == "fan" and room == 1:
        payload = "1" if val in (True, 1, "1", "on") else "0"
        topic = TOPIC_CMD_R1_FAN
    elif cmd == "ac" and room == 2:
        payload = "1" if val in (True, 1, "1", "on") else "0"
        topic = TOPIC_CMD_R2_AC
    else:
        return jsonify({"ok": False, "error": "unknown command"}), 400

    mqtt_client.publish(topic, payload, qos=0)
    return jsonify({"ok": True, "topic": topic, "payload": payload})


@app.route("/api/history_series")
def api_history_series():
    """Time series for client-side Plotly (HTML fragments with <script> do not run via innerHTML)."""
    conn = sqlite3.connect(db_name)
    try:
        cur = conn.cursor()
        cur.execute(
            """
            SELECT sensorID, timestamp, power FROM SensorData
            ORDER BY timestamp ASC
            LIMIT 12000
            """
        )
        points = [{"sensorID": int(r[0]), "t": str(r[1]), "p": int(r[2])} for r in cur.fetchall()]
    finally:
        conn.close()
    return jsonify({"points": points})


@app.route("/history/line_graph")
def line_graph():
    conn = sqlite3.connect(db_name)
    try:
        df = pd.read_sql_query("SELECT * FROM SensorData ORDER BY timestamp DESC LIMIT 5000", conn)
    finally:
        conn.close()
    if df.empty:
        return "<p>No data yet. Wait for mainProc to publish sensor/data.</p>"
    df = df.sort_values("timestamp")
    df["sensorID"] = df["sensorID"].map(lambda x: SENSOR_LABELS.get(int(x), str(x)))
    fig = px.line(
        df,
        x="timestamp",
        y="power",
        color="sensorID",
        title="Power (W) over time",
    )
    fig.update_layout(template="plotly_white", height=420)
    return fig.to_html(full_html=False, include_plotlyjs=False)


@app.route("/history/bar_graph")
def bar_graph():
    conn = sqlite3.connect(db_name)
    try:
        df = pd.read_sql_query(
            """
            SELECT sensorID, AVG(power) AS avg_power,
                   strftime('%Y-%m-%d %H:00:00', timestamp) AS hour
            FROM SensorData
            GROUP BY sensorID, hour
            ORDER BY hour
            """,
            conn,
        )
    finally:
        conn.close()
    if df.empty:
        return "<p>No data yet.</p>"
    df["sensorID"] = df["sensorID"].map(lambda x: SENSOR_LABELS.get(int(x), str(x)))
    fig = px.bar(
        df,
        x="hour",
        y="avg_power",
        color="sensorID",
        barmode="group",
        title="Average power (W) per hour",
    )
    fig.update_layout(template="plotly_white", height=420)
    return fig.to_html(full_html=True, include_plotlyjs="cdn")


@app.route("/api/current_power")
def current_power():
    """Latest row per sensor (for small gauges on History tab)."""
    conn = sqlite3.connect(db_name)
    cur = conn.cursor()
    cur.execute(
        """
        SELECT sensorID, power, timestamp FROM SensorData s1
        WHERE id = (
            SELECT MAX(id) FROM SensorData s2 WHERE s2.sensorID = s1.sensorID
        )
        """
    )
    rows = [{"sensorID": r[0], "power": r[1], "timestamp": r[2]} for r in cur.fetchall()]
    conn.close()
    return jsonify(rows)


if __name__ == "__main__":
    init_db()
    start_mqtt()
    socketio.run(app, host=web_host, port=web_port, allow_unsafe_werkzeug=True)
