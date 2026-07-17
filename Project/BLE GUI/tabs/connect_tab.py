import tkinter as tk
from bleak import BleakScanner, BleakClient

class ConnectTab(tk.Frame):
    def __init__(self, parent, app):
        super().__init__(parent)
        self.app = app
        FONT_SIZE_CONN = 12
        # =================== Connect Bluetooth Frame ===================
        self.button_frame_connect_btn = tk.Frame(self)
        self.button_frame_connect_btn.pack(padx=10, pady=5, anchor="w")

        self.scan_btn = tk.Button(self.button_frame_connect_btn, text="Scan", font=("Segoe UI", FONT_SIZE_CONN) ,command=self.on_scan)
        self.scan_btn.pack(side="left", padx=(0, 5), ipadx=10, ipady=3)

        self.connect_btn = tk.Button(self.button_frame_connect_btn, text="Connect", font=("Segoe UI", FONT_SIZE_CONN) , command=self.on_connect)
        self.connect_btn.pack(side="left", padx=(0, 5), ipadx=10, ipady=3)

        self.disconnect_btn = tk.Button(self.button_frame_connect_btn, text="Disconnect", font=("Segoe UI", FONT_SIZE_CONN) , command=self.on_disconnect)
        self.disconnect_btn.pack(side="left", padx=(0, 5), ipadx=10, ipady=3)

        self.device_list = tk.Listbox(self, font=("Segoe UI", 12))
        self.device_list.pack(fill="both", expand=True, padx=10)
        # =================== Connect Bluetooth Frame =================== 

    def notification_handler(self, sender, data):
        # msg = data.decode()
        # self.app.update_status(f"ESP32: {msg}")
        self.app.update_status(f"ESP32: Sensor ID={data[0]}, Status={hex(data[1])}")
        # self.app.update_status(f"Got {len(data)} bytes")
        sensor_id   = data[0]
        status      = data[1]
        
        if len(data) == 2:
            # Update status LED / button in SensorTab
            self.app.tab_sensor_control.update_sensor_status(sensor_id, status)
        # If the packet carries measurement payload, forward to chart parser
        if len(data) > 2:
            self.app.tab_sensor_control.parse_and_push(sensor_id, data)

    # --- Bridge giữa UI và Async ---
    def on_scan(self):
        self.app.run_async(self.scan_ble_logic())

    def on_connect(self):
        selected = self.device_list.get(tk.ACTIVE)
        if " - " in selected:
            address = selected.split(" - ")[1]
            self.app.run_async(self.connect_logic(address))

    def on_disconnect(self):
        self.app.run_async(self.disconnect_logic())

    def on_send(self):
        self.app.run_async(self.send_wifi_logic())

    def ota_update(self):
        pass

    async def scan_ble_logic(self):
        self.app.update_status("Scanning...")
        devices = await BleakScanner.discover()
        self.device_list.delete(0, tk.END)
        for d in devices:
            name = d.name if d.name else "Unknown"
            self.device_list.insert(tk.END, f"{name} - {d.address}")
        self.app.update_status("Scan complete")

    async def connect_logic(self, address):
        try:
            self.app.update_status(f"Connecting to {address}...")
            self.app.client = BleakClient(address)
            await self.app.client.connect()
            self.app.update_status("Connected!")
            
            # Register notify
            NOTIFY_UUID = "45fb1ea9-6f80-48a4-9337-4e8e4d455e2c"
            await self.app.client.start_notify(NOTIFY_UUID, self.notification_handler)
        except Exception as e:
            self.app.update_status(f"Error: {e}")

    async def disconnect_logic(self):
        try:
            if self.app.client and self.app.client.is_connected:
                await self.app.client.disconnect()
                self.app.update_status("Disconnected!")
            else:
                self.app.update_status("Not connected")
        except Exception as e:
            self.app.update_status(f"Disconnect error: {e}")
