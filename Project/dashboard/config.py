import os

BASE_DIR  = os.path.dirname(os.path.abspath(__file__))
CERTS_DIR = os.path.join(BASE_DIR, "..", "certs")

# ── AWS IoT Core ──────────────────────────────────────────────
AWS_ENDPOINT = "adiub9wc49ljw-ats.iot.ap-southeast-1.amazonaws.com"
AWS_PORT     = 8883
DEVICE_ID    = "esp32-001"

# MQTT topic phải khớp với TOPIC_SENSOR trong rtos_config.h
MQTT_TOPIC_SENSOR = f"devices/{DEVICE_ID}/sensor/bno055"

# ── Chứng chỉ TLS ─────────────────────────────────────────────
ROOT_CA_PATH = os.path.join(CERTS_DIR, "AmazonRootCA1.pem")
CERT_PATH    = os.path.join(CERTS_DIR, "device.crt")
KEY_PATH     = os.path.join(CERTS_DIR, "device.key")

# ── Dashboard ─────────────────────────────────────────────────
# DASHBOARD_CLIENT_ID: phải được phép trong AWS IoT Policy.
#
# Cách 1 - Nhanh (cho demo/exam): dùng cùng ID với ESP32.
#   CHÚ Ý: ESP32 sẽ bị ngắt kết nối khi dashboard đang chạy.
#   Đặt ESP32 offline trước khi mở dashboard, hoặc chỉ xem dữ liệu lịch sử.
#
# Cách 2 - Đúng (cho production): sửa AWS IoT Policy:
#   IoT Core Console → Security → Policies → <tên policy> → Edit
#   Đổi resource "arn:...:client/esp32-001" thành "arn:...:client/*"
#   Sau đó đổi DASHBOARD_CLIENT_ID = f"dashboard-{DEVICE_ID}"
DASHBOARD_CLIENT_ID  = "esp32-dashboard"  # Khớp wildcard esp32-* trong AWS IoT Policy
# DASHBOARD_CLIENT_ID = DEVICE_ID  # Chỉ dùng khi ESP32 offline

MAX_HISTORY          = 150   # Số điểm dữ liệu tối đa giữ trong bộ nhớ
REFRESH_INTERVAL_MS  = 500   # Chu kỳ làm mới (ms)
