"""
mqtt_subscriber.py
Chạy một MQTT subscriber trong luồng nền, kết nối AWS IoT Core qua TLS
mutual-auth và tích lũy dữ liệu BNO055 vào đối tượng SensorData.
"""

import json
import logging
import ssl
import threading
import time
from collections import deque

import paho.mqtt.client as mqtt
from paho.mqtt.enums import CallbackAPIVersion

import config

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("mqtt_subscriber")


class SensorData:
    """Lưu lịch sử dữ liệu cảm biến theo kiểu thread-safe."""

    def __init__(self, maxlen: int = config.MAX_HISTORY):
        self._lock = threading.Lock()
        self.timestamps: deque = deque(maxlen=maxlen)
        # Quaternion
        self.quat_w: deque = deque(maxlen=maxlen)
        self.quat_x: deque = deque(maxlen=maxlen)
        self.quat_y: deque = deque(maxlen=maxlen)
        self.quat_z: deque = deque(maxlen=maxlen)
        # Linear acceleration
        self.accel_x: deque = deque(maxlen=maxlen)
        self.accel_y: deque = deque(maxlen=maxlen)
        self.accel_z: deque = deque(maxlen=maxlen)
        # Gravity vector
        self.grav_x: deque = deque(maxlen=maxlen)
        self.grav_y: deque = deque(maxlen=maxlen)
        self.grav_z: deque = deque(maxlen=maxlen)

        self.latest: dict | None = None
        self.connected: bool = False
        self.message_count: int = 0
        self.last_error: str = ""

    def update(self, data: dict) -> None:
        with self._lock:
            self.timestamps.append(time.time())

            quat = data.get("quat", {})
            self.quat_w.append(float(quat.get("w", 0.0)))
            self.quat_x.append(float(quat.get("x", 0.0)))
            self.quat_y.append(float(quat.get("y", 0.0)))
            self.quat_z.append(float(quat.get("z", 0.0)))

            lin = data.get("lin_accel", {})
            self.accel_x.append(float(lin.get("x", 0.0)))
            self.accel_y.append(float(lin.get("y", 0.0)))
            self.accel_z.append(float(lin.get("z", 0.0)))

            grav = data.get("gravity", {})
            self.grav_x.append(float(grav.get("x", 0.0)))
            self.grav_y.append(float(grav.get("y", 0.0)))
            self.grav_z.append(float(grav.get("z", 0.0)))

            self.latest = data
            self.message_count += 1

    def snapshot(self) -> dict:
        """Trả về bản sao dữ liệu hiện tại (an toàn cho luồng)."""
        with self._lock:
            return {
                "timestamps":   list(self.timestamps),
                "quat":         {"w": list(self.quat_w), "x": list(self.quat_x),
                                 "y": list(self.quat_y), "z": list(self.quat_z)},
                "lin_accel":    {"x": list(self.accel_x), "y": list(self.accel_y),
                                 "z": list(self.accel_z)},
                "gravity":      {"x": list(self.grav_x),  "y": list(self.grav_y),
                                 "z": list(self.grav_z)},
                "latest":       self.latest,
                "connected":    self.connected,
                "message_count": self.message_count,
                "last_error":   self.last_error,
            }


# ── Singleton dữ liệu cảm biến (tồn tại xuyên suốt quá trình) ─────────
sensor_data = SensorData()
_started = False
_start_lock = threading.Lock()


def _on_connect(client, userdata, flags, reason_code, properties):
    """paho-mqtt v2 callback: reason_code là ReasonCode object."""
    if reason_code.is_failure:
        msg = f"MQTT CONNECT thất bại: {reason_code}"
        logger.error(msg)
        sensor_data.connected = False
        sensor_data.last_error = msg
    else:
        logger.info("MQTT kết nối thành công, subscribing %s", config.MQTT_TOPIC_SENSOR)
        sensor_data.connected = True
        sensor_data.last_error = ""
        client.subscribe(config.MQTT_TOPIC_SENSOR, qos=1)


def _on_disconnect(client, userdata, disconnect_flags, reason_code, properties):
    """paho-mqtt v2 callback."""
    sensor_data.connected = False
    msg = f"Disconnected: {reason_code}"
    logger.warning(msg)
    sensor_data.last_error = msg


def _on_subscribe(client, userdata, mid, reason_code_list, properties):
    """paho-mqtt v2 callback."""
    if reason_code_list and reason_code_list[0].is_failure:
        logger.error("Subscribe thất bại: %s", reason_code_list[0])
    else:
        logger.info("Subscribe thành công topic: %s", config.MQTT_TOPIC_SENSOR)


def _on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode("utf-8"))
        sensor_data.update(data)
    except (json.JSONDecodeError, UnicodeDecodeError, ValueError):
        pass


def start_mqtt_subscriber() -> None:
    """Khởi động luồng nền MQTT (chỉ khởi động một lần mỗi tiến trình)."""
    global _started
    with _start_lock:
        if _started:
            return
        _started = True

    client = mqtt.Client(
        callback_api_version=CallbackAPIVersion.VERSION2,
        client_id=config.DASHBOARD_CLIENT_ID,
    )
    client.on_connect    = _on_connect
    client.on_disconnect = _on_disconnect
    client.on_subscribe  = _on_subscribe
    client.on_message    = _on_message

    # Tự động reconnect: chờ 2s → 30s (exponential backoff)
    client.reconnect_delay_set(min_delay=2, max_delay=30)

    client.tls_set(
        ca_certs    = config.ROOT_CA_PATH,
        certfile    = config.CERT_PATH,
        keyfile     = config.KEY_PATH,
        tls_version = ssl.PROTOCOL_TLS_CLIENT,
    )

    def _run():
        retry_delay = 2
        while True:
            try:
                logger.info("Đang kết nối tới %s:%d client_id=%s",
                            config.AWS_ENDPOINT, config.AWS_PORT, config.DASHBOARD_CLIENT_ID)
                client.connect(config.AWS_ENDPOINT, config.AWS_PORT, keepalive=60)
                client.loop_forever(retry_first_connection=True)
                # loop_forever() trả về khi bị disconnect → chờ trước khi reconnect
                retry_delay = 2
            except Exception as exc:
                sensor_data.connected = False
                sensor_data.last_error = str(exc)
                logger.error("MQTT exception: %s — thử lại sau %ds", exc, retry_delay)
                time.sleep(retry_delay)
                retry_delay = min(retry_delay * 2, 30)

    t = threading.Thread(target=_run, daemon=True, name="mqtt-subscriber")
    t.start()
