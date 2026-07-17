import tkinter as tk
from tkinter import ttk
import functools
import struct
from bleak import BleakScanner, BleakClient
from enum import IntEnum

WRITE_SERVICE_UUID = "a1b2c3d4-e5f6-1728-394a-5b6c7d8e9f10"

# =================== Update GUI ===================
class Update_GUI:
    CONNECT_BUTTON = 0X00
    STOP = 0X01
    READ = 0X02
    MQTT_PUBLISH_OFF = 0X03
    MQTT_PUBLISH_ON = 0X04

# =================== Commands ===================
class Command(IntEnum):
    # Send type
    CASUAL = 1
    SAMPLE_RATE = 2
    READ = 3

    # Send command
    SET_SAMPLE_RATE = 127
    ALL_CONNECT = 128
    ALL_DISCONNECT = 129
    MQTT_PUBLISH = 130
    ALL_READ = 131
    ALL_STOP_READ = 132

# =================== Sensor tab ===================
class SensorControlTab(tk.Frame):
    def __init__(self, parent, app):
        super().__init__(parent)
        self.app = app

        self.sensor_commands = {
            "AHT20":  (0, 3),
            "BH1750": (1, 3),
            "BMP280": (2, 3),
            "BNO055": (3, 2),
            "INA226": (4, 5),
        }

        self.sample_rate_list = [50, 200, 500, 1000, 5000, 30000, 60000]    
        self.sample_rate_display = [
            f"{x/1000:g} s" if x >= 1000 else f"{x} ms"
            for x in self.sample_rate_list]

        self.canvases               = {}
        self.lights                 = {}
        self.buttons_conn           = {}
        self.buttons_read           = {}
        self.MAX_SENSOR = 5
        self.SEN_FONT_SIZE = 12

        # Tittle
        title_frame_status = tk.Frame(self)
        title_frame_status.pack(fill="x", padx=5, pady=2)
        title_frame_status.grid_columnconfigure(0, minsize=70)
        title_frame_status.grid_columnconfigure(1, minsize=80)
        title_frame_status.grid_columnconfigure(2, minsize=100)
        title_frame_status.grid_columnconfigure(3, minsize=50)
        title_frame_status.grid_columnconfigure(4, minsize=100)
        label_status = tk.Label(title_frame_status, text="Status", width=7, font=("Segoe UI", 12))
        label_status.grid(row=0, column=0, sticky="w", pady=2)
        label_device = tk.Label(title_frame_status, text="Device", width=7, font=("Segoe UI", 12))
        label_device.grid(row=0, column=1, sticky="w", pady=2)
        label_connect = tk.Label(title_frame_status, text="  Connect", width=7, font=("Segoe UI", 12))
        label_connect.grid(row=0, column=2, sticky="w", pady=2)
        label_read = tk.Label(title_frame_status, text="Read", width=7, font=("Segoe UI", 12))
        label_read.grid(row=0, column=3, sticky="w", pady=2)
        label_sample_rate = tk.Label(title_frame_status, anchor="w", text="Sample rate", width=12, font=("Segoe UI", 12))
        label_sample_rate.grid(row=0, column=4, sticky="w", pady=2)

        ttk.Separator(self, orient="horizontal").pack(anchor="w", ipadx=215, padx=5, pady=3)

        for name, (cmd, sample_rate) in self.sensor_commands.items():
            # Frame
            frame_status = tk.Frame(self)
            frame_status.pack(fill="x", padx=5, pady=2)
            frame_status.grid_columnconfigure(0, minsize=70)
            frame_status.grid_columnconfigure(1, minsize=80)
            frame_status.grid_columnconfigure(2, minsize=100)
            frame_status.grid_columnconfigure(3, minsize=50)
            frame_status.grid_columnconfigure(4, minsize=100)
            # Light status
            status_canvas = tk.Canvas(frame_status, width=20, height=20, highlightthickness=0)
            status_canvas.grid(row=0, column=0, padx=(0, 5), pady=2)
            light = status_canvas.create_oval(2, 2, 18, 18, fill="red", outline="darkred")
            # Label
            label = tk.Label(frame_status, text=name, anchor="center", width=7, font=("Segoe UI", 12))
            label.grid(row=0, column=1, sticky="w", pady=2)
            # Sample rate drop list
            sample_rate_drop = ttk.Combobox(frame_status, values=self.sample_rate_display, state="readonly", width=7)
            sample_rate_drop.current(sample_rate)
            sample_rate_drop.grid(row=0, column=4, padx=5, pady=2)
            sample_rate_drop.bind("<<ComboboxSelected>>", functools.partial(self.on_sample_rate_change, name))
            # Button
            connect_sensor_btn = tk.Button(frame_status, text="Connect", anchor="w", font=("Segoe UI", 12), command=functools.partial(self.on_send_command, name, Command.CASUAL, cmd))
            connect_sensor_btn.grid(row=0, column=2, padx=5, pady=2)
            read_sensor_btn = tk.Button(frame_status, text="Read", anchor="center", font=("Segoe UI", 12), command=functools.partial(self.on_send_command, name, Command.READ, cmd + self.MAX_SENSOR))
            read_sensor_btn.grid(row=0, column=3, padx=5, pady=2)
            # Save to array
            self.canvases[name]     = status_canvas
            self.lights[name]       = light
            self.buttons_conn[name] = connect_sensor_btn
            self.buttons_read[name] = read_sensor_btn

        ttk.Separator(self, orient="horizontal").pack(anchor="w", ipadx=215, padx=5, pady=3)

        # Connect all button
        frame_connect_all = tk.Frame(self)
        frame_connect_all.pack(fill="x", padx=5, pady=5)
        connect_all_sensor_btn = tk.Button(frame_connect_all, text="Connect All", width=19, font=("Segoe UI", 12), command=functools.partial(self.on_send_command, "CONNECT_ALL", Command.CASUAL, Command.ALL_CONNECT))
        connect_all_sensor_btn.pack(side="left")
        # Read all button
        read_all_sensor_btn = tk.Button(frame_connect_all, text="Read All", width=8, font=("Segoe UI", 12), command=functools.partial(self.on_send_command, "READ_ALL", Command.CASUAL, Command.ALL_READ))
        read_all_sensor_btn.pack(side="left")

        # Disconnect all button
        frame_disconnect_all = tk.Frame(self)
        frame_disconnect_all.pack(fill="x", padx=5, pady=5)
        disconnect_all_sensor_btn = tk.Button(frame_disconnect_all, text="Disconnect All", width=19, font=("Segoe UI", 12), command=functools.partial(self.on_send_command, "DISCONNECT_ALL", Command.CASUAL, Command.ALL_DISCONNECT))
        disconnect_all_sensor_btn.pack(side="left")
        # Stop read all button
        stop_read_all_sensor_btn = tk.Button(frame_disconnect_all, text="Stop All", width=8, font=("Segoe UI", 12), command=functools.partial(self.on_send_command, "STOP_READ_ALL", Command.CASUAL, Command.ALL_STOP_READ))
        stop_read_all_sensor_btn.pack(side="left")

        # MQTT button
        frame_mqtt = tk.Frame(self)
        frame_mqtt.pack(fill="x", padx=5, pady=5)
        mqtt_publish_btn = tk.Button(frame_mqtt, text="MQTT publish on", width=19, font=("Segoe UI", 12), command=functools.partial(self.on_send_command, "MQTT", Command.CASUAL, Command.MQTT_PUBLISH))
        mqtt_publish_btn.pack(side="left")
        self.button_mqtt = mqtt_publish_btn

    def on_send_command(self, name, command_type, command_int):
        if not self.app.client or not self.app.client.is_connected:
            # self.app.update_status("Error: Not connected!")
            return        
        
        self.app._run_async(self.send_command_logic(command_int))

    def on_sample_rate_change(self, name, event):
        index = event.widget.current()
        sample_rate = self.sample_rate_list[index]
        self.app._run_async(self.send_sample_rate_logic(Command.SET_SAMPLE_RATE, name, sample_rate))
        
    async def send_sample_rate_logic(self, command, name, sample_rate): 
        sensor_index = self.sensor_commands[name][0]
        print(f"Command {command}")
        print(f"Sensor ID {sensor_index}")
        print(f"Sample Rate {sample_rate}")
        data = struct.pack("<BBH", command, sensor_index, sample_rate)
        print(f"Data {data}")
        await self.app.client.write_gatt_char(WRITE_SERVICE_UUID, data)


    async def send_command_logic(self, command_int):
        # convert int to a byte
        data = bytes([command_int])
        await self.app.client.write_gatt_char(WRITE_SERVICE_UUID, data)
        # self.app.update_status("Command sent!")

    def update_sensor_status(self, sensor_id: int, status: int):
        # self.app.update_status(f"Got status: {hex(status)}")
        sensor_name = None
        for name, (cmd, _) in self.sensor_commands.items():
            if cmd == sensor_id:
                sensor_name = name
                break  
        
        if sensor_name and sensor_name in self.lights:
            canvas = self.canvases[sensor_name]
            light  = self.lights[sensor_name]
            sensor_btn_conn = self.buttons_conn[sensor_name]
            sensor_btn_read = self.buttons_read[sensor_name]
    
            if status == Update_GUI.CONNECT_BUTTON:
                self.app.root.after(0, lambda: canvas.itemconfig(light, fill="lightgreen", outline="darkgreen"))
                self.app.root.after(0, lambda: sensor_btn_conn.config(text="Disconnect"))
            elif status == Update_GUI.STOP:
                self.app.root.after(0, lambda: sensor_btn_read.config(text="Stop"))
            elif status == Update_GUI.READ:
                self.app.root.after(0, lambda: sensor_btn_read.config(text="Read"))
            else:
                self.app.root.after(0, lambda: canvas.itemconfig(light, fill="red", outline="darkred"))
                self.app.root.after(0, lambda: sensor_btn_conn.config(text="Connect"))

        # MQTT button change
        sensor_btn_mqtt = self.button_mqtt
        if status == Update_GUI.MQTT_PUBLISH_OFF:
            self.app.root.after(0, lambda: sensor_btn_mqtt.config(text="MQTT Publish Off"))
        elif status == Update_GUI.MQTT_PUBLISH_ON:
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