"""
app.py  –  ESP32 BNO055 IMU Real-time Dashboard
Chạy:  streamlit run app.py
"""

import datetime
import math
import time

import plotly.graph_objects as go
import streamlit as st

import config
from mqtt_subscriber import sensor_data, start_mqtt_subscriber

# ── Cấu hình trang ─────────────────────────────────────────────────────
st.set_page_config(
    page_title="ESP32 IMU Dashboard",
    page_icon="🛰️",
    layout="wide",
    initial_sidebar_state="collapsed",
)

# ── Khởi động MQTT (chỉ một lần mỗi tiến trình) ────────────────────────
start_mqtt_subscriber()

# ── Tiêu đề ─────────────────────────────────────────────────────────────
st.title("🛰️ ESP32 BNO055 IMU Dashboard")
st.caption(
    f"**Endpoint:** `{config.AWS_ENDPOINT}` &nbsp;|&nbsp; "
    f"**Device:** `{config.DEVICE_ID}` &nbsp;|&nbsp; "
    f"**Topic:** `{config.MQTT_TOPIC_SENSOR}`"
)

snap = sensor_data.snapshot()

# ── Thanh trạng thái ────────────────────────────────────────────────────
c1, c2, c3 = st.columns(3)
if snap["connected"]:
    c1.success("🟢  MQTT Đã kết nối")
else:
    err = snap.get("last_error", "")
    c1.error(f"🔴  MQTT Mất kết nối{': ' + err if err else ''}")
c2.metric("Tin nhắn đã nhận", snap["message_count"])
c3.metric("Điểm dữ liệu lưu trữ", len(snap["timestamps"]))

st.divider()

# ── Giá trị mới nhất ────────────────────────────────────────────────────
latest = snap["latest"]

if latest:
    quat = latest.get("quat", {})
    lin  = latest.get("lin_accel", {})
    grav = latest.get("gravity", {})

    w = float(quat.get("w", 1.0))
    x = float(quat.get("x", 0.0))
    y = float(quat.get("y", 0.0))
    z = float(quat.get("z", 0.0))

    # ── Chuyển Quaternion → Euler ───────────────────────────────
    sinr_cosp = 2.0 * (w * x + y * z)
    cosr_cosp = 1.0 - 2.0 * (x * x + y * y)
    roll  = math.degrees(math.atan2(sinr_cosp, cosr_cosp))

    sinp  = max(-1.0, min(1.0, 2.0 * (w * y - z * x)))
    pitch = math.degrees(math.asin(sinp))

    siny_cosp = 2.0 * (w * z + x * y)
    cosy_cosp = 1.0 - 2.0 * (y * y + z * z)
    yaw   = math.degrees(math.atan2(siny_cosp, cosy_cosp))

    # ── Đồng hồ đo Euler ────────────────────────────────────────
    st.subheader("Góc Euler  (tính từ Quaternion)")

    def _gauge(title: str, value: float, vmin: float, vmax: float, color: str):
        fig = go.Figure(go.Indicator(
            mode  = "gauge+number",
            value = value,
            title = {"text": title, "font": {"size": 16}},
            gauge = {
                "axis":      {"range": [vmin, vmax], "tickwidth": 1},
                "bar":       {"color": color},
                "bgcolor":   "white",
                "steps":     [{"range": [vmin, 0], "color": "#f0f0f0"},
                              {"range": [0, vmax],  "color": "#ddeeff"}],
                "threshold": {"line": {"color": "black", "width": 3},
                              "thickness": 0.8, "value": value},
            },
            number={"suffix": "°", "valueformat": ".1f"},
        ))
        fig.update_layout(height=220, margin=dict(l=20, r=20, t=50, b=10))
        return fig

    col_roll, col_pitch, col_yaw = st.columns(3)
    col_roll.plotly_chart(_gauge("Roll",  roll,  -180, 180, "#FF6B6B"), use_container_width=True)
    col_pitch.plotly_chart(_gauge("Pitch", pitch,  -90,  90, "#4ECDC4"), use_container_width=True)
    col_yaw.plotly_chart(_gauge("Yaw",   yaw,  -180, 180, "#45B7D1"), use_container_width=True)

    st.divider()

    # ── Số liệu tức thời ────────────────────────────────────────
    st.subheader("Quaternion  (giá trị mới nhất)")
    ca, cb, cc, cd = st.columns(4)
    ca.metric("W", f"{w:.5f}")
    cb.metric("X", f"{x:.5f}")
    cc.metric("Y", f"{y:.5f}")
    cd.metric("Z", f"{z:.5f}")

    st.subheader("Gia tốc tuyến tính  (m/s²)")
    ca, cb, cc = st.columns(3)
    ca.metric("X", f"{lin.get('x', 0):.3f}")
    cb.metric("Y", f"{lin.get('y', 0):.3f}")
    cc.metric("Z", f"{lin.get('z', 0):.3f}")

    st.subheader("Vector trọng lực  (m/s²)")
    ca, cb, cc = st.columns(3)
    ca.metric("X", f"{grav.get('x', 0):.3f}")
    cb.metric("Y", f"{grav.get('y', 0):.3f}")
    cc.metric("Z", f"{grav.get('z', 0):.3f}")

else:
    st.info("⏳  Đang chờ dữ liệu từ cảm biến BNO055...")

st.divider()

# ── Biểu đồ thời gian thực ─────────────────────────────────────────────
if snap["timestamps"]:
    times = [
        datetime.datetime.fromtimestamp(t).strftime("%H:%M:%S")
        for t in snap["timestamps"]
    ]
    axis_cfg = dict(tickangle=45, showgrid=True, gridcolor="#eeeeee")
    legend_cfg = dict(orientation="h", y=-0.25, font=dict(size=11))
    layout_defaults = dict(
        height=280,
        margin=dict(l=0, r=0, t=15, b=0),
        plot_bgcolor="white",
        paper_bgcolor="white",
    )

    col_l, col_r = st.columns(2)

    # Quaternion
    with col_l:
        st.subheader("Quaternion theo thời gian")
        fig_q = go.Figure()
        for name, data_y, color in [
            ("W", snap["quat"]["w"], "#FF6B6B"),
            ("X", snap["quat"]["x"], "#4ECDC4"),
            ("Y", snap["quat"]["y"], "#45B7D1"),
            ("Z", snap["quat"]["z"], "#96CEB4"),
        ]:
            fig_q.add_trace(go.Scatter(x=times, y=data_y, name=name,
                                        line=dict(color=color, width=1.5)))
        fig_q.update_layout(**layout_defaults, yaxis_range=[-1.1, 1.1],
                             xaxis=axis_cfg, legend=legend_cfg)
        st.plotly_chart(fig_q, use_container_width=True)

    # Gia tốc tuyến tính
    with col_r:
        st.subheader("Gia tốc tuyến tính theo thời gian  (m/s²)")
        fig_a = go.Figure()
        for name, data_y, color in [
            ("X", snap["lin_accel"]["x"], "#FF6B6B"),
            ("Y", snap["lin_accel"]["y"], "#4ECDC4"),
            ("Z", snap["lin_accel"]["z"], "#45B7D1"),
        ]:
            fig_a.add_trace(go.Scatter(x=times, y=data_y, name=name,
                                        line=dict(color=color, width=1.5)))
        fig_a.update_layout(**layout_defaults, xaxis=axis_cfg, legend=legend_cfg)
        st.plotly_chart(fig_a, use_container_width=True)

    col_l2, col_r2 = st.columns(2)

    # Trọng lực
    with col_l2:
        st.subheader("Vector trọng lực theo thời gian  (m/s²)")
        fig_g = go.Figure()
        for name, data_y, color in [
            ("X", snap["gravity"]["x"], "#FF6B6B"),
            ("Y", snap["gravity"]["y"], "#4ECDC4"),
            ("Z", snap["gravity"]["z"], "#45B7D1"),
        ]:
            fig_g.add_trace(go.Scatter(x=times, y=data_y, name=name,
                                        line=dict(color=color, width=1.5)))
        fig_g.update_layout(**layout_defaults, xaxis=axis_cfg, legend=legend_cfg)
        st.plotly_chart(fig_g, use_container_width=True)

    # Raw JSON
    with col_r2:
        st.subheader("Dữ liệu thô (JSON)")
        if latest:
            st.json(latest, expanded=True)

st.divider()
st.caption(
    f"Tự động làm mới mỗi **{config.REFRESH_INTERVAL_MS} ms** &nbsp;|&nbsp; "
    "ESP32 BNO055 NDOF → AWS IoT Core → Dashboard"
)

# ── Tự động làm mới ─────────────────────────────────────────────────────
time.sleep(config.REFRESH_INTERVAL_MS / 1000.0)
st.rerun()
