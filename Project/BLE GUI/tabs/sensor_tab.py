import tkinter as tk
import functools
import struct
from bleak import BleakScanner, BleakClient

# =================== Sensor tab ===================
class SensorControlTab(tk.Frame):
    def __init__(self, parent, app):
        super().__init__(parent)
        self.app = app

        self.sensor_commands = {
            "AHT20":  0,
            "BH1750": 1,
            "BMP280": 2,
            "BNO055": 3,
            "INA226": 4,
        }
        self.canvases       = {}
        self.lights         = {}
        self.buttons_conn   = {}
        self.buttons_read   = {}
        self.MAX_SENSOR = 5
        self.SEN_FONT_SIZE = 12
        for name, cmd in self.sensor_commands.items():
            # Frame
            frame_status = tk.Frame(self)
            frame_status.pack(fill="x", padx=5, pady=2)
            frame_status.grid_columnconfigure(2, minsize=100)
            frame_status.grid_columnconfigure(3, minsize=50)
            # Light status
            status_canvas = tk.Canvas(frame_status, width=20, height=20, highlightthickness=0)
            status_canvas.grid(row=0, column=0, padx=(0, 5), pady=2)
            light = status_canvas.create_oval(2, 2, 18, 18, fill="red", outline="darkred")
            # Label
            label = tk.Label(frame_status, text=name, anchor="w", width=7, font=("Segoe UI", 12))
            label.grid(row=0, column=1, sticky="w", pady=2)
            # Button
            connect_sensor_btn = tk.Button(frame_status, text="Connect", font=("Segoe UI", 12), command=functools.partial(self.on_send_command, name, cmd))
            connect_sensor_btn.grid(row=0, column=2, padx=5, pady=2)
            read_sensor_btn = tk.Button(frame_status, text="Read", font=("Segoe UI", 12), command=functools.partial(self.on_send_command, name, cmd + self.MAX_SENSOR))
            read_sensor_btn.grid(row=0, column=3, padx=5, pady=2)
            # Save to array
            self.canvases[name]     = status_canvas
            self.lights[name]       = light
            self.buttons_conn[name] = connect_sensor_btn
            self.buttons_read[name] = read_sensor_btn

        # Connect all button
        frame_connect_all = tk.Frame(self)
        frame_connect_all.pack(fill="x", padx=5, pady=5)
        connect_all_sensor_btn = tk.Button(frame_connect_all, text="Connect All", width=19, font=("Segoe UI", 12), command=functools.partial(self.on_send_command, "all", 128))
        connect_all_sensor_btn.pack(side="left")

        # Disconnect all button
        frame_disconnect_all = tk.Frame(self)
        frame_disconnect_all.pack(fill="x", padx=5, pady=5)
        disconnect_all_sensor_btn = tk.Button(frame_disconnect_all, text="Disconnect All", width=19, font=("Segoe UI", 12), command=functools.partial(self.on_send_command, "all", 129))
        disconnect_all_sensor_btn.pack(side="left")

        # MQTT button
        frame_mqtt = tk.Frame(self)
        frame_mqtt.pack(fill="x", padx=5, pady=5)
        mqtt_publish_btn = tk.Button(frame_mqtt, text="MQTT publish on", width=19, font=("Segoe UI", 12), command=functools.partial(self.on_send_command, "mqtt", 130))
        mqtt_publish_btn.pack(side="left")
        self.button_mqtt = mqtt_publish_btn


    def on_send_command(self, sensor_name, command_int):
        if not self.app.client or not self.app.client.is_connected:
            # self.app.update_status("Error: Not connected!")
            return
        
        self.app._run_async(self.send_command_logic(sensor_name, command_int))

    async def send_command_logic(self, sensor_name, command_int):
        try:
            # convert int to a byte
            WRITE_UUID = "a1b2c3d4-e5f6-1728-394a-5b6c7d8e9f10"
            data = bytes([command_int])
            await self.app.client.write_gatt_char(WRITE_UUID, data)
            # self.app.update_status("Command sent!")
        except Exception as e:
            # self.app.update_status(f"Error: {e}")
            pass

    def update_sensor_status(self, sensor_id: int, status: int):
        # self.app.update_status(f"Got status: {hex(status)}")
        sensor_name = None
        for name, cmd in self.sensor_commands.items():
            if cmd == sensor_id:
                sensor_name = name
                break  
        
        if sensor_name and sensor_name in self.lights:
            canvas = self.canvases[sensor_name]
            light  = self.lights[sensor_name]
            sensor_btn_conn = self.buttons_conn[sensor_name]
            sensor_btn_read = self.buttons_read[sensor_name]
    
            if status == 0x00:
                self.app.root.after(0, lambda: canvas.itemconfig(light, fill="lightgreen", outline="darkgreen"))
                self.app.root.after(0, lambda: sensor_btn_conn.config(text="Disconnect"))
            elif status == 0x01:
                self.app.root.after(0, lambda: sensor_btn_read.config(text="Stop"))
            elif status == 0x02:
                self.app.root.after(0, lambda: sensor_btn_read.config(text="Read"))
            else:
                self.app.root.after(0, lambda: canvas.itemconfig(light, fill="red", outline="darkred"))
                self.app.root.after(0, lambda: sensor_btn_conn.config(text="Connect"))

        # MQTT button change
        sensor_btn_mqtt = self.button_mqtt
        if status == 0x03:
            self.app.root.after(0, lambda: sensor_btn_mqtt.config(text="MQTT Publish Off"))
        elif status == 0x04:
            self.app.root.after(0, lambda: sensor_btn_mqtt.config(text="MQTT Publish On"))
            
    # Parse sensor payload and push values to ChartTab
    def parse_and_push(self, sensor_id: int, payload: bytes):
        chart = getattr(self.app, "tab_chart", None)
        if chart is None:
            return

        try:
            if sensor_id == 11 and len(payload) >= 3:   # AHT20
                hum, = struct.unpack_from("<B", payload, 2)
                chart.push("Humidity (%)", hum) 

            elif sensor_id == 12 and len(payload) >= 6:  # BH1750
                lux, = struct.unpack_from("<f", payload, 2)
                chart.push("Lux", lux)

            elif sensor_id == 13 and len(payload) >= 10:  # BMP280
                pres, temp = struct.unpack_from("<ff", payload, 2)
                chart.push_batch({"Pressure (hPa)":pres, "Temp (°C)": temp})

            elif sensor_id == 14 and len(payload) >= 6:  # BNO055
                ax, = struct.unpack_from("<f", payload, 2)
                chart.push("Accel", ax)

            elif sensor_id == 15 and len(payload) >= 6:   # INA226
                volt, = struct.unpack_from("<f", payload, 2)
                chart.push("Battery (%)", volt)

        except struct.error as e:
            # self.app.update_status(f"Parse error: {e}")
            pass
# =================== Sensor tab ===================