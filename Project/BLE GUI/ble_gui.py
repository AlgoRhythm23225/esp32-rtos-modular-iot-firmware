import asyncio
import threading
import queue
import tkinter as tk
from tkinter import ttk
import json
from datetime import datetime
from tkinter import filedialog as fd
from tkinter import messagebox
import os
from tabs.sensor_tab import SensorControlTab
from tabs.chart_tab import ChartTab
from bleak import BleakClient, BleakScanner

# ── UUIDs  ──────────────────────────────────
SERVICE_UUID   = "76c3e5d6-ee9e-4f48-bb4d-287d88f872d6"
CHAR_SSID_UUID = "6fc254b7-6bc0-475b-b836-720d2287922d"   # wifi_ssi_uuid
CHAR_PW_UUID   = "cfedbbc2-0598-4581-b1fd-b7d54fec0396"   # wifi_pw_uuid
CHAR_CMD_UUID  = "9823ae51-1729-4f51-b847-112233445566"   # prov_cmd_uuid
CHAR_OTA_CTRL_UUID = "6fc254b7-6bc0-475b-b836-720d2287222d" # OTA Control UUID (Reversed)
CHAR_OTA_DATA_UUID = "cfedbbc2-0593-4581-b1fd-b7d54fcc0396" # OTA Data UUID (Reversed)
CHAR_SENSOR_DATA_UUID = "6a1b2c3d-4e5f-6a7b-8c9d-0e1f2a3b4c5d" # data_val_handle
CHAR_SENSOR_CONFIG_UUID = "b1c2d3e4-f5a6-b7c8-d9e0-f1a2b3c4d5e6" # config_val_handle

_STOP_SENTINEL = None 

class BLEGuiApp:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("ESP32 BLE WiFi Provisioning")
        self.root.geometry("1280x720")
        self.root.minsize(860, 520)

        self.client: BleakClient | None = None
        self.connected      = False
        self.is_connecting  = False
        self.manual_disconnect = False
        self.scanned_devices: dict = {}
        self.watchdog_timer = None
        self.current_report_rate = 0
        self.ota_total_bytes = 0
        self.ota_sent_bytes = 0

        # ── Logic phân cấp Device Logs ────────────────
        self.log_buffer = [] 
        self.level_map = {
            "ALL": 0,         
            "VERBOSE": 1,
            "DEBUG": 2,
            "INFO": 3,
            "WARNING": 4,
            "ERROR": 5,
            "NONE": 6         
        }
        self.current_log_level = 0  # Mặc định là ALL

        # ── asyncio event loop ─────────
        self.ble_loop = asyncio.new_event_loop()
        threading.Thread(target=self._run_ble_loop, daemon=True).start()

        # ── Send queue + worker thread ────────────────────────────────────
        self.send_queue: queue.Queue = queue.Queue()
        threading.Thread(
            target=self._send_worker, daemon=True, name="BLE-SendWorker"
        ).start()

        self._build_ui()

    # ── UI ───────────────────────────────────────────────────────────────────
    def _build_ui(self):
        self.notebook = ttk.Notebook(self.root)
        self.notebook.pack(fill="both", expand=True, padx=5, pady=5)

        self.tab_provisioning = ttk.Frame(self.notebook, padding=12)
        self.tab_sensors = ttk.Frame(self.notebook, padding=12)
        self.tab_ota = ttk.Frame(self.notebook, padding=12)
        self.tab_logs = ttk.Frame(self.notebook, padding=12)
        self.tab_sensor_control = SensorControlTab(self.notebook, self)
        self.tab_chart = ChartTab(self.notebook, self)

        self.notebook.add(self.tab_provisioning, text="Provisioning")
        self.notebook.add(self.tab_sensors, text="Sensors")
        self.notebook.add(self.tab_ota, text="OTA Update")
        self.notebook.add(self.tab_logs, text="Device Logs")
        self.notebook.add(self.tab_sensor_control, text="Sensors")
        self.notebook.add(self.tab_chart, text="Charts")

        # ==============================================================
        # TAB 1: PROVISIONING
        # ==============================================================
        main = self.tab_provisioning 

        vcmd32 = (self.root.register(lambda v: len(v) <= 32), '%P')
        vcmd64 = (self.root.register(lambda v: len(v) <= 64), '%P')

        ttk.Label(main, text="Select Device :").place(x=0, y=0)
        self.device_selector = ttk.Combobox(main, state="readonly", width=48)
        self.device_selector.place(x=115, y=0)
        self.device_selector.set("Click 'Scan' to find devices...")
        ttk.Button(main, text="Scan", command=self.scan_device).place(x=460, y=-2)
        
        self.toggle_conn_btn = ttk.Button(main, text="Connect", command=self.connect_device)
        self.toggle_conn_btn.place(x=550, y=-2)

        ttk.Label(main, text="SSID (max 32):").place(x=0, y=34)
        self.ssid_var = tk.StringVar()
        ttk.Entry(main, textvariable=self.ssid_var, width=46,
                  validate="key", validatecommand=vcmd32).place(x=115, y=34)

        ttk.Label(main, text="Password (8-64):").place(x=0, y=64)
        self.pw_var = tk.StringVar()
        ttk.Entry(main, textvariable=self.pw_var, width=46,
                  validate="key", validatecommand=vcmd64).place(x=115, y=64)

        self.send_wifi_btn = ttk.Button(
            main, text="Send Wi-Fi", command=self.enqueue_credentials, state="disabled", width=12
        )
        self.send_wifi_btn.place(x=450, y=62)

        self.trigger_conn_btn = ttk.Button(
            main, text="Connect", command=self.trigger_wifi_connection, state="disabled", width=10
        )
        self.trigger_conn_btn.place(x=550, y=62)

        ttk.Label(main, text="Queue:").place(x=0, y=94)
        self.queue_var = tk.StringVar(value="Queue empty")
        ttk.Label(main, textvariable=self.queue_var, foreground="gray").place(x=115, y=94)

        # -- Trạng thái tổng hợp (BLE, Wi-Fi, Internet) --
        ttk.Label(main, text="BLE Status:").place(x=0, y=124)
        self.status_var = tk.StringVar(value="DISCONNECTED")
        self.status_label = ttk.Label(
            main, textvariable=self.status_var,
            font=("Arial", 10, "bold"), foreground="red"
        )
        self.status_label.place(x=70, y=124)

        ttk.Label(main, text="Wi-Fi:").place(x=220, y=124)
        self.sensor_wifi_var = tk.StringVar(value="--")
        self.wifi_status_label = ttk.Label(main, textvariable=self.sensor_wifi_var, font=("Arial", 10, "bold"), foreground="gray")
        self.wifi_status_label.place(x=260, y=124)

        ttk.Label(main, text="Internet:").place(x=400, y=124)
        self.sensor_net_var = tk.StringVar(value="--")
        self.net_status_label = ttk.Label(main, textvariable=self.sensor_net_var, font=("Arial", 10, "bold"), foreground="gray")
        self.net_status_label.place(x=455, y=124)

        self.log_text = tk.Text(main, height=14, width=75) 
        self.log_text.tag_config("error",   foreground="red")
        self.log_text.tag_config("success", foreground="green")
        self.log_text.tag_config("info",    foreground="blue")
        self.log_text.tag_config("queue",   foreground="purple")
        self.log_text.place(x=0, y=155)
        self._add_copy_menu(self.log_text)

        # ==============================================================
        # TAB 2: SENSORS 
        # ==============================================================
        ttk.Label(self.tab_sensors, text="Sensors Manager", font=("Arial", 14, "bold")).pack(pady=15)

        # -- KHUNG HIỂN THỊ BẢNG (GRID) --
        table_frame = ttk.Frame(self.tab_sensors)
        table_frame.pack(fill="x", padx=10, pady=10)

        # Configure columns to have a fixed minimum size to prevent layout shifting
        table_frame.columnconfigure(0, minsize=150)
        table_frame.columnconfigure(1, minsize=100)
        table_frame.columnconfigure(2, minsize=80)
        table_frame.columnconfigure(3, minsize=260)
        table_frame.columnconfigure(4, minsize=220)

        headers = ["Sensor", "Address", "Status", "Latest Data", "Actions"]
        for col, h in enumerate(headers):
            ttk.Label(table_frame, text=h, font=("Arial", 10, "bold")).grid(row=0, column=col, padx=10, pady=5, sticky="w")

        # Dòng 1: Cảm biến DS3231
        self.ds3231_addr_var = tk.StringVar(value="--")
        self.ds3231_sts_var = tk.StringVar(value="--")
        self.ds3231_data_var = tk.StringVar(value="--")

        ttk.Label(table_frame, text="DS3231 (RTC+Temp)", width=20, anchor="w").grid(row=1, column=0, padx=10, pady=10, sticky="w")
        ttk.Label(table_frame, textvariable=self.ds3231_addr_var, foreground="gray", width=12, anchor="w").grid(row=1, column=1, padx=10, pady=10, sticky="w")
        self.ds3231_sts_label = ttk.Label(table_frame, textvariable=self.ds3231_sts_var, foreground="gray", width=8, anchor="w")
        self.ds3231_sts_label.grid(row=1, column=2, padx=10, pady=10, sticky="w")
        ttk.Label(table_frame, textvariable=self.ds3231_data_var, font=("Arial", 9, "bold"), width=32, anchor="w").grid(row=1, column=3, padx=10, pady=10, sticky="w")

        action_frame_1 = ttk.Frame(table_frame)
        action_frame_1.grid(row=1, column=4, padx=10, pady=10, sticky="w")
        self.btn_get_rtc = ttk.Button(action_frame_1, text="GET", width=5, command=lambda: self.send_sensor_req("Get", 0), state="disabled")
        self.btn_get_rtc.pack(side="left", padx=2)
        self.btn_set_rtc = ttk.Button(action_frame_1, text="SET TIME", width=13, command=self.open_rtc_config_window, state="disabled")
        self.btn_set_rtc.pack(side="left", padx=2)
        self.btn_retry_sensor = ttk.Button(action_frame_1, text="RETRY", width=8, command=lambda: self.send_sensor_req("Retry", 0), state="disabled")
        self.btn_retry_sensor.pack(side="left", padx=2)

        # -- KHUNG CÀI ĐẶT REPORTING --
        rep_frame = ttk.LabelFrame(self.tab_sensors, text=" Auto Reporting Config (Background) ", padding=10)
        rep_frame.pack(fill="x", padx=10, pady=20)

        ttk.Label(rep_frame, text="Interval (sec):").grid(row=0, column=0, sticky="w", padx=(0,5))
        self.rep_rate_var = tk.StringVar(value="1")
        tk.Spinbox(rep_frame, from_=0, to=3600, textvariable=self.rep_rate_var, width=4).grid(row=0, column=1, sticky="w", padx=(0,15))

        ttk.Label(rep_frame, text="Data Type:").grid(row=0, column=2, sticky="w", padx=(0,5))
        self.rep_type_cb = ttk.Combobox(rep_frame, state="readonly", width=12)
        self.rep_type_cb['values'] = ("Sensor Only",)
        self.rep_type_cb.current(0)
        self.rep_type_cb.grid(row=0, column=3, sticky="w", padx=(0,15))

        self.btn_set_rep = ttk.Button(rep_frame, text="Apply Auto Config", command=self.send_report_config, state="disabled")
        self.btn_set_rep.grid(row=0, column=4, sticky="w")

        # ==============================================================
        # TAB 3: OTA UPDATE 
        # ==============================================================
        ttk.Label(self.tab_ota, text="Firmware OTA Update", font=("Arial", 14, "bold")).pack(pady=10)
        
        # --- Khung BLE OTA ---
        ble_ota_frame = ttk.LabelFrame(self.tab_ota, text=" BLE OTA (Local File) ", padding=10)
        ble_ota_frame.pack(fill="x", padx=10, pady=5)
        self.ota_file_path = None
        self.btn_select_bin = ttk.Button(ble_ota_frame, text="Select .bin File", command=self.select_ota_file)
        self.btn_select_bin.pack(pady=5)
        self.ota_file_label = ttk.Label(ble_ota_frame, text="No file selected", foreground="gray")
        self.ota_file_label.pack(pady=5)
        self.btn_start_ota = ttk.Button(ble_ota_frame, text="Start BLE OTA", command=self.start_ota, state="disabled")
        self.btn_start_ota.pack(pady=5)

        # --- Khung Cloud OTA ---
        cloud_ota_frame = ttk.LabelFrame(self.tab_ota, text=" Cloud OTA (S3 / HTTP URL) ", padding=10)
        cloud_ota_frame.pack(fill="x", padx=10, pady=5)
        
        ttk.Label(cloud_ota_frame, text="URL:").grid(row=0, column=0, sticky="w", pady=5)
        self.cloud_url_var = tk.StringVar()
        ttk.Entry(cloud_ota_frame, textvariable=self.cloud_url_var, width=80).grid(row=0, column=1, padx=5, pady=5)
        
        ttk.Label(cloud_ota_frame, text="Version:").grid(row=1, column=0, sticky="w", pady=5)
        self.cloud_ver_var = tk.StringVar(value="1.0.1")
        ttk.Entry(cloud_ota_frame, textvariable=self.cloud_ver_var, width=15).grid(row=1, column=1, sticky="w", padx=5, pady=5)

        self.btn_start_cloud_ota = ttk.Button(cloud_ota_frame, text="Trigger Cloud OTA", command=self.start_cloud_ota, state="disabled")
        self.btn_start_cloud_ota.grid(row=1, column=1, sticky="e", padx=5, pady=5)

        # --- Khung Firmware Comparison ---
        comparison_frame = ttk.LabelFrame(self.tab_ota, text=" Firmware Comparison ", padding=10)
        comparison_frame.pack(fill="x", padx=10, pady=5)

        ttk.Label(comparison_frame, text="Property", font=("Arial", 9, "bold")).grid(row=0, column=0, sticky="w", padx=5, pady=2)
        ttk.Label(comparison_frame, text="Running Firmware (ESP32)", font=("Arial", 9, "bold")).grid(row=0, column=1, sticky="w", padx=15, pady=2)
        ttk.Label(comparison_frame, text="Incoming OTA (New File)", font=("Arial", 9, "bold")).grid(row=0, column=2, sticky="w", padx=15, pady=2)
        ttk.Separator(comparison_frame, orient='horizontal').grid(row=1, column=0, columnspan=3, sticky="ew", pady=5)

        self.ota_info_proj = tk.StringVar(value="--")
        self.ota_new_proj = tk.StringVar(value="--")
        self.ota_info_app_ver = tk.StringVar(value="--")
        self.ota_new_ver = tk.StringVar(value="--")
        self.ota_info_cur_part = tk.StringVar(value="--")
        self.ota_new_part = tk.StringVar(value="--")
        self.ota_info_size = tk.StringVar(value="--")
        self.ota_new_size = tk.StringVar(value="--")
        self.ota_info_method = tk.StringVar(value="N/A")
        self.ota_new_method = tk.StringVar(value="--")

        ttk.Label(comparison_frame, text="Project:").grid(row=2, column=0, sticky="w", padx=5)
        ttk.Label(comparison_frame, textvariable=self.ota_info_proj).grid(row=2, column=1, sticky="w", padx=15)
        ttk.Label(comparison_frame, textvariable=self.ota_new_proj).grid(row=2, column=2, sticky="w", padx=15)

        ttk.Label(comparison_frame, text="Version:").grid(row=3, column=0, sticky="w", padx=5)
        ttk.Label(comparison_frame, textvariable=self.ota_info_app_ver, foreground="green").grid(row=3, column=1, sticky="w", padx=15)
        ttk.Label(comparison_frame, textvariable=self.ota_new_ver, foreground="blue").grid(row=3, column=2, sticky="w", padx=15)

        ttk.Label(comparison_frame, text="Partition:").grid(row=4, column=0, sticky="w", padx=5)
        ttk.Label(comparison_frame, textvariable=self.ota_info_cur_part).grid(row=4, column=1, sticky="w", padx=15)
        ttk.Label(comparison_frame, textvariable=self.ota_new_part).grid(row=4, column=2, sticky="w", padx=15)

        ttk.Label(comparison_frame, text="Size:").grid(row=5, column=0, sticky="w", padx=5)
        ttk.Label(comparison_frame, textvariable=self.ota_info_size).grid(row=5, column=1, sticky="w", padx=15)
        ttk.Label(comparison_frame, textvariable=self.ota_new_size).grid(row=5, column=2, sticky="w", padx=15)

        ttk.Label(comparison_frame, text="OTA Method:").grid(row=6, column=0, sticky="w", padx=5)
        ttk.Label(comparison_frame, textvariable=self.ota_info_method).grid(row=6, column=1, sticky="w", padx=15)
        ttk.Label(comparison_frame, textvariable=self.ota_new_method).grid(row=6, column=2, sticky="w", padx=15)

        ctrl_frame = ttk.Frame(comparison_frame)
        ctrl_frame.grid(row=2, column=3, rowspan=5, padx=40)
        self.btn_get_ota_info = ttk.Button(ctrl_frame, text="Get Device Info", command=lambda: self.send_sensor_req("GetOtaInfo", -1), state="disabled", width=18)
        self.btn_get_ota_info.pack(pady=2)
        self.btn_reboot_device = ttk.Button(ctrl_frame, text="Restart ESP32", command=self.confirm_reboot, state="disabled", width=18)
        self.btn_reboot_device.pack(pady=2)

        # --- Progress Bar chung ---
        self.ota_progress = ttk.Progressbar(self.tab_ota, orient="horizontal", length=500, mode="determinate")
        self.ota_progress.pack(pady=10)
        self.ota_percent_label = ttk.Label(self.tab_ota, text="0% (0 / 0 bytes)", font=("Arial", 10))
        self.ota_percent_label.pack(pady=2)

        self.ota_feedback_label = ttk.Label(self.tab_ota, text="Status: Ready", font=("Arial", 10, "bold"), foreground="blue")
        self.ota_feedback_label.pack(pady=5)

        # ==============================================================
        # TAB 4: DEVICE LOGS 
        # ==============================================================
        log_ctrl_frame = ttk.Frame(self.tab_logs)
        log_ctrl_frame.pack(fill="x", padx=10, pady=10)

        ttk.Label(log_ctrl_frame, text="Level:").pack(side="left", padx=(0, 5))
        
        self.log_level_cb = ttk.Combobox(log_ctrl_frame, state="readonly", width=45)
        self.log_level_cb['values'] = ("ALL", "VERBOSE", "DEBUG", "INFO", "WARNING", "ERROR", "NONE")
        self.log_level_cb.current(0) 
        self.log_level_cb.pack(side="left", padx=(0, 10))

        self.log_level_cb.bind("<<ComboboxSelected>>", lambda e: self._set_log_level())

        self.set_level_btn = ttk.Button(log_ctrl_frame, text="Set", width=6, command=self._set_log_level)
        self.set_level_btn.pack(side="left", padx=5)

        self.clear_log_btn = ttk.Button(log_ctrl_frame, text="Clear", width=6, command=self._clear_device_logs)
        self.clear_log_btn.pack(side="left", padx=5)

        self.device_log_text = tk.Text(
            self.tab_logs, height=20, width=80, 
            bg="white", fg="black", font=("Consolas", 10)
        )
        self.device_log_text.pack(padx=10, pady=(0, 10))
        
        self.device_log_text.tag_config("I", foreground="darkgreen")  
        self.device_log_text.tag_config("D", foreground="blue")       
        self.device_log_text.tag_config("W", foreground="#d97706")    
        self.device_log_text.tag_config("E", foreground="red")        
        self.device_log_text.tag_config("V", foreground="gray")       
        self._add_copy_menu(self.device_log_text)

    def _add_copy_menu(self, widget):
        menu = tk.Menu(widget, tearoff=0)
        menu.add_command(label="Copy", command=lambda: widget.event_generate("<<Copy>>"))
        
        def show_menu(event):
            menu.tk_popup(event.x_root, event.y_root)
            
        widget.bind("<Button-3>", show_menu)
        
        # Make read-only but allow Ctrl+C and navigation
        def read_only(e):
            if e.state & 0x0004 and e.keysym.lower() == 'c': return None
            if e.keysym in ("Up", "Down", "Left", "Right", "Prior", "Next", "Home", "End"): return None
            return "break"
        widget.bind("<Key>", read_only)
    # ── Logging (Provisioning) ──────────────────────────────────────────────
    def _log(self, text: str, tag: str | None = None):
        def _append():
            self.log_text.insert("end", f"> {text}\n", tag)
            self.log_text.see("end")
        self.root.after(0, _append)

    def _update_queue_label(self):
        size = self.send_queue.qsize()
        text = f"{size} item(s) pending" if size else "Queue empty"
        self.root.after(0, lambda: self.queue_var.set(text))

    # ── Logic Device Logs (LỌC CHÍNH XÁC - EXACT MATCH) ─────────────────────
    def _set_log_level(self):
        selected_level = self.log_level_cb.get()
        self.current_log_level = self.level_map.get(selected_level, 0)
        self._refresh_device_logs() 

    def add_device_log(self, level_str: str, tag: str, msg: str):
        lvl_val = self.level_map.get(level_str, 1)
        self.log_buffer.append((lvl_val, tag, msg))
        
        # CHỈ HIỂN THỊ NẾU CHỌN 'ALL' (0) HOẶC TRÙNG KHỚP CHÍNH XÁC
        if self.current_log_level == 0 or lvl_val == self.current_log_level:
            self.device_log_text.insert("end", msg + "\n", tag)
            self.device_log_text.see("end")

    def _refresh_device_logs(self):
        self.device_log_text.delete("1.0", "end")
        for lvl_val, tag, msg in self.log_buffer:
            # KIỂM TRA TRÙNG KHỚP CHÍNH XÁC KHI VẼ LẠI
            if self.current_log_level == 0 or lvl_val == self.current_log_level: 
                self.device_log_text.insert("end", msg + "\n", tag)
        self.device_log_text.see("end")

    def _clear_device_logs(self):
        self.log_buffer.clear()
        self.device_log_text.delete("1.0", "end")




    # ── asyncio loop ─────────────────────────────────────────────────────────
    def _run_ble_loop(self):
        asyncio.set_event_loop(self.ble_loop)
        self.ble_loop.run_forever()

    def _run_async(self, coro):
        return asyncio.run_coroutine_threadsafe(coro, self.ble_loop)

    # ── Send worker ──────────────────────────────────────────────────────────
    def _send_worker(self):
        while True:
            item = self.send_queue.get()          

            if item is _STOP_SENTINEL:
                self.send_queue.task_done()
                break

            char_uuid, payload_bytes, label = item
            if not label.startswith("OTA_CHUNK_") or self.send_queue.qsize() % 16 == 0:
                self._update_queue_label()

            if not (self.client and self.client.is_connected):
                self._log(f"[Queue] Not connected. Clearing {self.send_queue.qsize() + 1} pending packets from queue.", "error")
                self.send_queue.task_done()
                while not self.send_queue.empty():
                    try:
                        self.send_queue.get_nowait()
                        self.send_queue.task_done()
                    except queue.Empty:
                        break
                self._update_queue_label()
                continue

            future = asyncio.run_coroutine_threadsafe(
                self._write_char(char_uuid, payload_bytes), self.ble_loop
            )
            try:
                future.result(timeout=10)
                if not label.startswith("OTA_CHUNK_"):
                    self._log(f"[Queue] '{label}' sent ({len(payload_bytes)}B)", "success")
                
                if char_uuid == CHAR_OTA_DATA_UUID:
                    self.ota_sent_bytes += len(payload_bytes)
                    sent = self.ota_sent_bytes
                    total = self.ota_total_bytes
                    # Throttle GUI updates for progress bar (every ~4KB or at the end)
                    if sent % 4096 == 0 or sent >= total:
                        self.root.after(0, lambda s=sent, t=total: self._update_ota_progress(s, t))
            except Exception as e:
                self._log(f"[Queue] '{label}' failed: {e}", "error")
                if char_uuid in (CHAR_OTA_DATA_UUID, CHAR_OTA_CTRL_UUID):
                    self.root.after(0, lambda: self.btn_start_ota.config(state="normal"))
            finally:
                self.send_queue.task_done()
                if not label.startswith("OTA_CHUNK_") or self.send_queue.qsize() % 16 == 0:
                    self._update_queue_label()

    async def _write_char(self, char_uuid: str, data: bytes):
        # Khắc phục lỗi "Multiple Characteristics" do Windows lưu Cache cũ
        target_char = None
        for service in self.client.services:
            for char in service.characteristics:
                if char.uuid.lower() == char_uuid.lower():
                    target_char = char
                    break
            if target_char:
                break
                
        if target_char is None:
            raise Exception(f"Characteristic {char_uuid} not found!")
            
        if char_uuid.lower() == CHAR_OTA_DATA_UUID.lower():
            await self.client.write_gatt_char(target_char, data, response=False)
            await asyncio.sleep(0.01) # Tốc độ ~25KB/s để ESP32 kịp ghi Flash mà không rớt gói
        else:
            await self.client.write_gatt_char(target_char, data, response=True)
            # Nếu là lệnh khởi tạo OTA (gửi Hash), cần nghỉ 1.5 giây để ESP32 có thời gian xóa Flash (esp_ota_begin)
            if len(data) in (32, 36) and char_uuid.lower() == CHAR_OTA_CTRL_UUID.lower():
                await asyncio.sleep(1.5)

    # ── Enqueue ──────────────────────────────────────────────────────────────
    def enqueue_credentials(self):
        ssid = self.ssid_var.get().strip()
        pw   = self.pw_var.get().strip()

        if not ssid:
            self._log("Error: SSID cannot be empty.", "error")
            return
        if len(pw) < 8:
            self._log(f"Error: Password too short ({len(pw)}/min 8).", "error")
            return

        self.send_queue.put((CHAR_SSID_UUID, ssid.encode("utf-8"), f"SSID={ssid}"))
        self.send_queue.put((CHAR_PW_UUID,   pw.encode("utf-8"),   "PASSWORD"))

        self._log(f"[Queue] +2 packets — SSID:\"{ssid}\"  PW:{pw}", "queue")
        self._update_queue_label()

    def trigger_wifi_connection(self):
        self.send_queue.put((CHAR_CMD_UUID, b"\x01", "CMD_CONNECT"))
        self._log("[Queue] +1 packet — Command: Trigger Wi-Fi Connection", "queue")
        self._update_queue_label()

    def send_sensor_req(self, req_type, sensor_id):
        payload_dict = {"Req": req_type, "Sensor": sensor_id}
        payload_bytes = json.dumps(payload_dict).encode("utf-8")
        self.send_queue.put((CHAR_SENSOR_CONFIG_UUID, payload_bytes, f"CMD_{req_type}_S{sensor_id}"))
        self._log(f"[Queue] +1 packet — {req_type} request for Sensor {sensor_id}", "queue")
        self._update_queue_label()
        self._reset_watchdog()

    def open_rtc_config_window(self):
        config_win = tk.Toplevel(self.root)
        config_win.title("Set RTC Time")
        config_win.geometry("350x150")
        config_win.resizable(False, False)
        config_win.transient(self.root)
        config_win.grab_set()

        cfg_year_var = tk.StringVar(value=datetime.now().strftime("%Y"))
        cfg_month_var = tk.StringVar(value=datetime.now().strftime("%m"))
        cfg_date_var = tk.StringVar(value=datetime.now().strftime("%d"))
        cfg_hour_var = tk.StringVar(value=datetime.now().strftime("%H"))
        cfg_min_var = tk.StringVar(value=datetime.now().strftime("%M"))
        cfg_sec_var = tk.StringVar(value=datetime.now().strftime("%S"))

        def send_time():
            try:
                payload_dict = {
                    "Req": "Set", "Sensor": 0,
                    "Y": int(cfg_year_var.get()), "Mo": int(cfg_month_var.get()), "D": int(cfg_date_var.get()),
                    "H": int(cfg_hour_var.get()), "M": int(cfg_min_var.get()), "S": int(cfg_sec_var.get())
                }
                payload_bytes = json.dumps(payload_dict).encode("utf-8")
                self.send_queue.put((CHAR_SENSOR_CONFIG_UUID, payload_bytes, "CMD_SET_RTC_TIME"))
                self._log(f"[Queue] +1 packet — Set RTC Time", "queue")
                self._update_queue_label()
                self._reset_watchdog()
                config_win.destroy()
            except ValueError:
                self._log("Invalid Date/Time format.", "error")

        def sync_from_pc():
            now = datetime.now()
            cfg_year_var.set(f"{now.year:04d}")
            cfg_month_var.set(f"{now.month:02d}")
            cfg_date_var.set(f"{now.day:02d}")
            cfg_hour_var.set(f"{now.hour:02d}")
            cfg_min_var.set(f"{now.minute:02d}")
            cfg_sec_var.set(f"{now.second:02d}")

        main_frame = ttk.Frame(config_win, padding=10)
        main_frame.pack(fill="both", expand=True)

        ttk.Label(main_frame, text="Date (Y-M-D):").grid(row=0, column=0, sticky="w", pady=5)
        date_frame = ttk.Frame(main_frame)
        date_frame.grid(row=0, column=1, sticky="w")
        tk.Spinbox(date_frame, from_=2000, to=2099, textvariable=cfg_year_var, width=4).pack(side="left")
        tk.Spinbox(date_frame, from_=1, to=12, textvariable=cfg_month_var, width=2, format="%02.0f").pack(side="left", padx=2)
        tk.Spinbox(date_frame, from_=1, to=31, textvariable=cfg_date_var, width=2, format="%02.0f").pack(side="left")

        ttk.Label(main_frame, text="Time (H:M:S):").grid(row=1, column=0, sticky="w", pady=5)
        time_frame = ttk.Frame(main_frame)
        time_frame.grid(row=1, column=1, sticky="w")
        tk.Spinbox(time_frame, from_=0, to=23, textvariable=cfg_hour_var, width=2, format="%02.0f").pack(side="left")
        tk.Spinbox(time_frame, from_=0, to=59, textvariable=cfg_min_var, width=2, format="%02.0f").pack(side="left", padx=2)
        tk.Spinbox(time_frame, from_=0, to=59, textvariable=cfg_sec_var, width=2, format="%02.0f").pack(side="left")

        btn_frame = ttk.Frame(main_frame)
        btn_frame.grid(row=2, column=0, columnspan=2, pady=10, sticky="e")
        ttk.Button(btn_frame, text="Sync from PC", command=sync_from_pc).pack(side="left", padx=5)
        ttk.Button(btn_frame, text="Apply", command=send_time).pack(side="left")

    def send_report_config(self):
        try:
            rate = int(self.rep_rate_var.get())
        except ValueError:
            self._log("Invalid Interval.", "error")
            return

        type_str = self.rep_type_cb.get()
        type_val = 1 # Bây giờ chỉ có 1 lựa chọn

        payload_dict = {"Req": "Report", "Rate": rate, "Type": type_val}
        payload_bytes = json.dumps(payload_dict).encode("utf-8")

        self.current_report_rate = rate
        self.send_queue.put((CHAR_SENSOR_CONFIG_UUID, payload_bytes, f"CONFIG_REPORT Rate:{rate}s Type:{type_val}"))
        self._log(f"[Queue] +1 packet — Report Config (Rate: {rate}s, Type: {type_str})", "queue")
        self._update_queue_label()
        self._reset_watchdog()

    # ── Scan ─────────────────────────────────────────────────────────────────
    def scan_device(self):
        self._log("Scanning (4s)...", "info")
        self._run_async(self._scan_async())

    async def _scan_async(self):
        try:
            devices = await BleakScanner.discover(timeout=4.0)
            self.scanned_devices = {
                (d.name.strip() if d.name and d.name.strip() else d.address): d
                for d in devices
            }
            keys = list(self.scanned_devices.keys())
            self.root.after(0, lambda: self.device_selector.config(values=keys))
            if keys:
                self.root.after(0, lambda: self.device_selector.current(0))
                self._log(f"Found {len(keys)} device(s).", "success")
            else:
                self._log("No devices found.", "error")
        except Exception as e:
            self._log(f"Scan error: {e}", "error")

    # ── Connect ───────────────────────────────────────────────────────────────
    def connect_device(self):
        if self.is_connecting or self.connected:
            return
        selection = self.device_selector.get()
        if selection not in self.scanned_devices:
            self._log("Please scan and select a device first.", "error")
            return
        self.is_connecting = True
        self.manual_disconnect = False
        
        self.toggle_conn_btn.config(state="disabled")
        
        self.status_var.set("CONNECTING...")
        self.status_label.config(foreground="orange")
        self._run_async(self._connect_async(self.scanned_devices[selection]))

    async def _connect_async(self, device):
        try:
            self._log(f"Connecting to {device.address}...", "info")
            # Khai báo callback ngay lúc khởi tạo để tương thích với mọi phiên bản Bleak
            self.client = BleakClient(device, disconnected_callback=self._on_disconnect)
            await asyncio.wait_for(self.client.connect(), timeout=7.0)
            
            self.connected = True
            self._log(f"Connected to {device.name or device.address}", "success")
            
            # Chủ động báo cho giao diện (GUI) biết đã kết nối
            self.root.after(0, self._ui_set_connected)
            
            # Tìm Characteristic của Sensor và đăng ký nhận Notify
            target_char = None
            for service in self.client.services:
                for char in service.characteristics:
                    if char.uuid.lower() == CHAR_SENSOR_DATA_UUID.lower():
                        target_char = char
                        break
                if target_char:
                    break
            
            if target_char:
                await self.client.start_notify(target_char, self._notification_handler)
                self._log("Subscribed to Sensor Data notifications.", "success")
                
                # Tự động gửi lệnh GET để lấy Address và Status 1 lần duy nhất khi kết nối
                self.send_sensor_req("Get", 0)
                self.send_sensor_req("Get", 1) # Lấy trạng thái mạng hiện tại

            NOTIFY_UUID = "a1b2c3d4-e5f6-1728-394a-5b6c7d8e9f10"
            await self.client.start_notify(NOTIFY_UUID, self._notification_handler)
                
        except asyncio.TimeoutError:
            self._log("Connection timed out.", "error")
            self.root.after(0, self._ui_set_disconnected)
        except Exception as e:
            self._log(f"Connection failed: {e}", "error")
            self.root.after(0, self._ui_set_disconnected)
        finally:
            self.is_connecting = False
            
    # ── Xử lý khi nhận được gói tin Notify từ ESP32 ─────────────────────────
    def _notification_handler(self, sender, data: bytes):
        sensor_id, status = data[0], data[1]
        if len(data) == 2:
            # Update status LED / button in SensorTab
            self.tab_sensor_control.update_sensor_status(sensor_id, status)
        if len(data) > 2:
            self.tab_sensor_control.parse_and_push(sensor_id, data)

        try:
            payload_str = data.decode("utf-8", errors="ignore")
            payload = json.loads(payload_str)
            
            if "OtaStatus" in payload:
                status = payload["OtaStatus"]
                def show_ota_result():
                    if status == "Success":
                        self.ota_feedback_label.config(text="Status: Success! Please restart.", foreground="green")
                        messagebox.showinfo("OTA Success", "Firmware updated successfully!\nPlease click 'Restart ESP32' to reboot.")
                    else:
                        self.ota_feedback_label.config(text="Status: Failed! (Corrupt or Size Mismatch)", foreground="red")
                        messagebox.showerror("OTA Failed", "Update failed due to corrupt file or size mismatch.\nDevice aborted OTA to protect the system!")
                    
                    # Reset UI states
                    self.btn_start_ota.config(state="normal")
                    self.btn_start_cloud_ota.config(state="normal")
                    self.ota_progress["value"] = 0
                    self.ota_percent_label.config(text="0% (0 / 0 bytes)")
                    
                self.root.after(0, show_ota_result)
                return

            sensor_id = payload.get("Sensor", -1)
            
            def update_ui():
                # Ưu tiên xử lý gói tin trạng thái mạng
                if "NetSts" in payload:
                    net_status = payload["NetSts"]
                    w_state = net_status.get("Wifi")
                    if w_state == 1:
                        self.sensor_wifi_var.set("Connected")
                        self.wifi_status_label.config(foreground="green")
                    elif w_state == 2:
                        self.sensor_wifi_var.set("Connecting...")
                        self.wifi_status_label.config(foreground="orange")
                    elif w_state == 3:
                        self.sensor_wifi_var.set("Failed (Wrong PW?)")
                        self.wifi_status_label.config(foreground="red")
                    else:
                        self.sensor_wifi_var.set("Disconnected")
                        self.wifi_status_label.config(foreground="red")
                    
                    if net_status.get("Net"):
                        self.sensor_net_var.set("Available")
                        self.net_status_label.config(foreground="green")
                    else:
                        self.sensor_net_var.set("Offline")
                        self.net_status_label.config(foreground="red")

                elif sensor_id == 0:
                    if "Addr" in payload:
                        self.ds3231_addr_var.set(payload["Addr"])
                    
                    if "Sts" in payload:
                        sts = payload["Sts"]
                        self.ds3231_sts_var.set(sts)
                        if sts == "OK":
                            self.ds3231_sts_label.config(foreground="green")
                        elif sts == "ERR":
                            self.ds3231_sts_label.config(foreground="red")
                        else:
                            self.ds3231_sts_label.config(foreground="gray")
                    
                    if "T" in payload:
                        temp = payload.get("T", "--")
                        time_val = payload.get("Time", "--")
                        self.ds3231_data_var.set(f"{temp} °C  |  {time_val}")
                
                elif "OtaProgress" in payload:
                    prog = payload["OtaProgress"]
                    w = prog.get("W", 0)
                    t = prog.get("T", 0)
                    self._update_ota_progress(w, t)
                
                elif "OtaInfo" in payload:
                    info = payload["OtaInfo"]
                    self.ota_info_proj.set(info.get("Proj", "--"))
                    self.ota_info_app_ver.set(info.get("AppVer", "--"))
                    cur_part = info.get("CurPart", "--")
                    next_part = info.get("NextPart", "--")
                    part_size = info.get("PartSize", 0)
                    
                    self.ota_info_cur_part.set(cur_part)
                    self.ota_new_part.set(next_part)
                    if part_size > 0:
                        size_mb = part_size / (1024 * 1024)
                        self.ota_info_size.set(f"Unknown (Max: {part_size} bytes / {size_mb:.1f} MB)")
                    else:
                        self.ota_info_size.set("Unknown")
                
                self._reset_watchdog()
                
            self.root.after(0, update_ui)
            
        except Exception as e:
            print(f"Error parsing sensor data: {e}")

    def _reset_watchdog(self):
        if self.watchdog_timer:
            self.root.after_cancel(self.watchdog_timer)
        if self.connected:
            if self.current_report_rate == 0:
                return # Không kích hoạt watchdog nếu người dùng đặt Interval = 0 (dừng báo cáo)
            timeout = max(5000, (self.current_report_rate + 5) * 1000)
            self.watchdog_timer = self.root.after(timeout, self._watchdog_timeout)

    def _watchdog_timeout(self):
        if self.connected:
            self._log("Connection timeout (no data from ESP32).", "error")
            self.disconnect_device()

    def _ui_set_connected(self):
        self.status_var.set("CONNECTED")
        self.status_label.config(foreground="green")
        self.device_selector.config(state="disabled")
        self.send_wifi_btn.config(state="normal")
        self.trigger_conn_btn.config(state="normal")
        self.btn_get_rtc.config(state="normal")
        self.btn_set_rtc.config(state="normal")
        self.btn_retry_sensor.config(state="normal")
        self.btn_set_rep.config(state="normal")
        if self.ota_file_path: self.btn_start_ota.config(state="normal")
        self.btn_start_cloud_ota.config(state="normal")
        self.btn_get_ota_info.config(state="normal")
        self.btn_reboot_device.config(state="normal")
        self.toggle_conn_btn.config(text="Disconnect", command=self.disconnect_device, state="normal")
        self._reset_watchdog()

    def _ui_set_disconnected(self):
        self.status_var.set("DISCONNECTED")
        self.status_label.config(foreground="red")
        self.sensor_wifi_var.set("--")
        self.wifi_status_label.config(foreground="gray")
        self.sensor_net_var.set("--")
        self.net_status_label.config(foreground="gray")
        self.ds3231_addr_var.set("--")
        self.ds3231_sts_var.set("--")
        self.ds3231_sts_label.config(foreground="gray")
        self.ds3231_data_var.set("--")
        self.device_selector.config(state="readonly")
        self.send_wifi_btn.config(state="disabled")
        self.trigger_conn_btn.config(state="disabled")
        self.btn_get_rtc.config(state="disabled")
        self.btn_set_rtc.config(state="disabled")
        self.btn_retry_sensor.config(state="disabled")
        self.btn_set_rep.config(state="disabled")
        self.btn_start_ota.config(state="disabled")
        self.btn_start_cloud_ota.config(state="disabled")
        self.btn_get_ota_info.config(state="disabled")
        self.btn_reboot_device.config(state="disabled")
        
        self.ota_info_app_ver.set("--")
        self.ota_info_proj.set("--")
        self.ota_info_cur_part.set("--")
        self.ota_new_part.set("--")
        self.ota_info_size.set("--")
        self.ota_info_method.set("N/A")
        
        self.toggle_conn_btn.config(text="Connect", command=self.connect_device, state="normal")
        if self.watchdog_timer:
            self.root.after_cancel(self.watchdog_timer)
            self.watchdog_timer = None

        # Clear queue on disconnect to prevent stale transfers/logging
        while not self.send_queue.empty():
            try:
                self.send_queue.get_nowait()
                self.send_queue.task_done()
            except queue.Empty:
                break
        self._update_queue_label()

    def _on_disconnect(self, client):
        self.connected = False
        self._log("Device disconnected", "error" if not self.manual_disconnect else "info")
        self.root.after(0, self._ui_set_disconnected)

    # ── Disconnect ────────────────────────────────────────────────────────────
    def disconnect_device(self):
        if not self.client:
            return
        self.manual_disconnect = True
        self.toggle_conn_btn.config(state="disabled") 
        self._run_async(self._disconnect_async())

    async def _disconnect_async(self):
        try:
            if self.client:
                try:
                    await self._write_char(CHAR_CMD_UUID, b"\x02") # Gửi lệnh đá kết nối
                except:
                    pass
                await self.client.disconnect()
        finally:
            self.client = None

    # ── Cleanup ───────────────────────────────────────────────────────────────
    def on_close(self):
        # Ẩn giao diện ngay lập tức để người dùng có cảm giác app đã tắt tức thì
        self.root.withdraw()
        
        # Chủ động gửi lệnh ngắt kết nối BLE xuống hệ điều hành trước khi thoát
        if self.client and self.connected:
            try:
                async def graceful_disconnect():
                    if self.client.is_connected:
                        try:
                            await self._write_char(CHAR_CMD_UUID, b"\x02")
                        except:
                            pass
                        await self.client.disconnect()
                future = asyncio.run_coroutine_threadsafe(graceful_disconnect(), self.ble_loop)
                future.result(timeout=2.0) 
            except Exception as e:
                print(f"Error disconnecting on exit: {e}")
                
        self.send_queue.put(_STOP_SENTINEL)
        self.ble_loop.call_soon_threadsafe(self.ble_loop.stop)
        self.root.destroy()

    # ── OTA Process ───────────────────────────────────────────────────────────
    def _update_ota_progress(self, sent, total):
        self.ota_progress['maximum'] = total
        self.ota_progress['value'] = sent
        if total > 0:
            pct = (sent / total) * 100
            self.ota_percent_label.config(text=f"{pct:.1f}% ({sent} / {total} bytes)")
            if sent >= total:
                self.btn_start_ota.config(state="normal")
        else:
            self.ota_percent_label.config(text="0% (0 / 0 bytes)")

    def select_ota_file(self):
        filepath = fd.askopenfilename(
            title="Select Firmware Bin",
            filetypes=[("Binary Files", "*.bin"), ("All Files", "*.*")]
        )
        if filepath:
            filename = os.path.basename(filepath)
            try:
                with open(filepath, "rb") as f:
                    data = f.read(256)
                    if len(data) >= 176:
                        import struct
                        # esp_app_desc_t nằm ở byte số 32 (0x20)
                        magic = struct.unpack_from("<I", data, 32)[0]
                        if magic == 0xABCD5432:
                            app_ver = data[48:80].split(b'\0')[0].decode('utf-8', 'ignore').strip()
                            proj_name = data[80:112].split(b'\0')[0].decode('utf-8', 'ignore').strip()
                            file_size = os.path.getsize(filepath)
                            size_mb = file_size / (1024 * 1024)
                            
                            self.ota_file_path = filepath
                            self.ota_new_ver.set(app_ver)
                            self.ota_new_proj.set(proj_name)
                            self.ota_new_size.set(f"{file_size} bytes ({size_mb:.2f} MB)")
                            self.ota_new_method.set("BLE (Local File)")
                            
                            self.ota_file_label.config(
                                text=f"Selected: {filename}",
                                foreground="blue"
                            )
                            if self.connected:
                                self.btn_start_ota.config(state="normal")
                        else:
                            self.ota_file_path = None
                            self.ota_file_label.config(text=f"Error: Not a valid ESP32 firmware bin file!", foreground="red")
                            self.btn_start_ota.config(state="disabled")
                    else:
                        self.ota_file_path = None
                        self.ota_file_label.config(text=f"Error: File too small!", foreground="red")
                        self.btn_start_ota.config(state="disabled")
            except Exception as e:
                self.ota_file_path = None
                self.ota_file_label.config(text=f"Error reading file: {e}", foreground="red")
                self.btn_start_ota.config(state="disabled")

    def confirm_reboot(self):
        if messagebox.askyesno("Confirm Reboot", "Are you sure you want to reboot the ESP32?"):
            self.send_sensor_req("Reboot", -1)
            self._log("Sent Reboot command to ESP32", "info")

    def start_ota(self):
        if not self.ota_file_path or not self.connected:
            return
            
        old_ver = self.ota_info_app_ver.get()
        if old_ver == "--":
            messagebox.showerror("Update Error", "You must click 'Get Device Info' to fetch current firmware details before updating!")
            return
            
        new_ver = self.ota_new_ver.get() if hasattr(self, "ota_new_ver") else "Unknown"
        
        msg = f"Are you sure you want to flash the new Firmware?\n\n- Running Version: {old_ver}\n- Incoming Version: {new_ver}"
        if not messagebox.askyesno("Confirm OTA Update", msg):
            return
            
        self.btn_start_ota.config(state="disabled")
        self._log(f"Starting OTA update: {os.path.basename(self.ota_file_path)}", "info")
        
        chunk_size = 256 # MTU an toàn cho BLE
        try:
            with open(self.ota_file_path, "rb") as f:
                firmware_data = f.read()
                
            self.ota_total_bytes = len(firmware_data)
            self.ota_sent_bytes = 0
            self.root.after(0, lambda: self._update_ota_progress(0, self.ota_total_bytes))

            # TÍNH TOÁN MÃ BĂM SHA-256 VÀ GỬI KÈM SIZE CỦA FILE BIN
            import hashlib, struct
            file_hash = hashlib.sha256(firmware_data).digest()
            self._log(f"Firmware SHA-256: {file_hash.hex()}", "info")
            # Nén: 32 bytes (Hash) + 4 bytes (Size, Little Endian) = 36 bytes
            payload = file_hash + struct.pack('<I', len(firmware_data))
            
            self.ota_feedback_label.config(text="Status: Erasing Flash...", foreground="orange")
            self.send_queue.put((CHAR_OTA_CTRL_UUID, payload, "OTA_START_HASH"))
                
            total_size = len(firmware_data)
            self.ota_feedback_label.config(text="Status: Flashing...", foreground="blue")
            for i in range(0, total_size, chunk_size):
                chunk = firmware_data[i:i+chunk_size]
                self.send_queue.put((CHAR_OTA_DATA_UUID, chunk, f"OTA_CHUNK_{i}"))
                
            # Gửi lệnh FINISH tới OTA_CTRL để báo hiệu kết thúc
            self.send_queue.put((CHAR_OTA_CTRL_UUID, bytes([0x02]), "OTA_CMD_FINISH"))
            self._log(f"[Queue] Added {total_size//chunk_size + 1} OTA chunks to Queue.", "queue")
            
        except Exception as e:
            self._log(f"Failed to read OTA file: {e}", "error")

    def start_cloud_ota(self):
        url = self.cloud_url_var.get().strip()
        version = self.cloud_ver_var.get().strip()
        if not url:
            self._log("Error: Cloud OTA URL cannot be empty.", "error")
            return
        
        self.btn_start_cloud_ota.config(state="disabled")
        self.btn_start_ota.config(state="disabled")
        self._log(f"Triggering Cloud OTA with URL: {url}", "info")
        
        payload_dict = {"Req": "OtaCloud", "Url": url, "Version": version}
        payload_bytes = json.dumps(payload_dict).encode("utf-8")
        
        self.send_queue.put((CHAR_SENSOR_CONFIG_UUID, payload_bytes, "CMD_OTA_CLOUD"))
        self._log("[Queue] +1 packet — Cloud OTA Trigger", "queue")
        self._update_queue_label()
        self._reset_watchdog()

if __name__ == "__main__":
    root = tk.Tk()
    app = BLEGuiApp(root)
    root.protocol("WM_DELETE_WINDOW", app.on_close)
    root.mainloop()