import tkinter as tk
from tkinter import ttk
from collections import deque
import matplotlib
matplotlib.use("TkAgg")
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import time

# Dark theme palette
BG_COLOR     = "#0f1117"
PANEL_COLOR  = "#1a1d27"
GRID_COLOR   = "#2a2d3a"
TEXT_COLOR   = "#c8ccd8"
ACCENT      = "#3d7fff"

# Per-channel colors
COLORS = {
    "Battery (%)":    "#f0c040",
    "Humidity (%)":   "#40c0f0",
    "Temp (°C)":      "#f07040",
    "Pressure (hPa)": "#a070f0",
    "Accel":          "#40f090",
    "Lux":            "#ffe060",
}

WINDOW = 120   # number of points to keep

class ChartTab(tk.Frame):
    def __init__(self, parent, app):
        super().__init__(parent, bg=BG_COLOR)
        self.app = app
        self._running = True

        # --- Data buffers ---
        self.buffers = {k: deque([0.0] * WINDOW, maxlen=WINDOW) for k in COLORS}
        self.timestamps = deque(range(-WINDOW, 0), maxlen=WINDOW)

        # --- Build figure ---
        plt.rcParams.update({
            "axes.facecolor":   PANEL_COLOR,
            "figure.facecolor": BG_COLOR,
            "axes.edgecolor":   GRID_COLOR,
            "axes.labelcolor":  TEXT_COLOR,
            "xtick.color":      TEXT_COLOR,
            "ytick.color":      TEXT_COLOR,
            "grid.color":       GRID_COLOR,
            "text.color":       TEXT_COLOR,
            "lines.linewidth":  1.5,
            "font.size":        10,
        })

        self.fig = plt.figure(figsize=(10, 7), dpi=100, facecolor=BG_COLOR)
        gs = gridspec.GridSpec(4, 1, figure=self.fig,
                               hspace=0.55,
                               top=0.95, bottom=0.06,
                               left=0.08, right=0.97)

        self.ax_multi   = self.fig.add_subplot(gs[0])   # battery + humidity + temp
        self.ax_press   = self.fig.add_subplot(gs[1])   # pressure
        self.ax_accel   = self.fig.add_subplot(gs[2])   # accel
        self.ax_lux     = self.fig.add_subplot(gs[3])   # lux

        self._setup_ax(self.ax_multi,  "Battery / Humidity / Temp")
        self._setup_ax(self.ax_press,  "Pressure")
        self._setup_ax(self.ax_accel,  "Acceleration")
        self._setup_ax(self.ax_lux,    "Illuminance")

        # --- Create lines ---
        xs = list(self.timestamps)

        self.lines = {}
        # Bat - Hum - Temp
        for key in ["Battery (%)", "Humidity (%)", "Temp (°C)"]:
            ln, = self.ax_multi.plot(xs, list(self.buffers[key]),
                                     color=COLORS[key], label=key)
            self.lines[key] = ln
        self.ax_multi.legend(loc="upper left", fontsize=10,
                             framealpha=0.3, ncol=3)
        
        # Press
        ln, = self.ax_press.plot(xs, list(self.buffers["Pressure (hPa)"]),
                                 color=COLORS["Pressure (hPa)"], label="Pressure (hPa)")
        self.lines["Pressure (hPa)"] = ln
        self.ax_press.legend(loc="upper left", fontsize=10, framealpha=0.3)

        # Accel
        ln, = self.ax_accel.plot(xs, list(self.buffers["Accel"]),
                                     color=COLORS["Accel"], label="Accel")
        self.lines["Accel"] = ln
        self.ax_accel.legend(loc="upper left", fontsize=10, framealpha=0.3)
        
        # Lux
        ln, = self.ax_lux.plot(xs, list(self.buffers["Lux"]),
                               color=COLORS["Lux"], label="Lux")
        self.lines["Lux"] = ln
        self.ax_lux.legend(loc="upper left", fontsize=10, framealpha=0.3)

        # --- Embed in Tk ---
        self.canvas_widget = FigureCanvasTkAgg(self.fig, master=self)
        self.canvas_widget.get_tk_widget().pack(fill="both", expand=True)

        # --- Controls bar ---
        ctrl = tk.Frame(self, bg=BG_COLOR)
        ctrl.pack(fill="x", padx=8, pady=(0, 6))

        tk.Label(ctrl, text="Window:", bg=BG_COLOR, fg=TEXT_COLOR,
                 font=("Helvetica", 10)).pack(side="left")
        self.window_var = tk.IntVar(value=WINDOW)
        tk.Scale(ctrl, from_=20, to=300, orient="horizontal",
                 variable=self.window_var, bg=BG_COLOR, fg=TEXT_COLOR,
                 troughcolor=PANEL_COLOR, highlightthickness=0,
                 command=self._on_window_change,
                 length=140, showvalue=True, font=("Helvetica", 10)
                 ).pack(side="left", padx=(2, 12))

        self.pause_var = tk.BooleanVar(value=False)
        tk.Checkbutton(ctrl, text="Pause", variable=self.pause_var,
                       bg=BG_COLOR, fg=TEXT_COLOR, selectcolor=PANEL_COLOR,
                       activebackground=BG_COLOR, font=("Helvetica", 10)
                       ).pack(side="left", padx=4)

        tk.Button(ctrl, text="Clear", bg=PANEL_COLOR, fg=TEXT_COLOR,
                  relief="flat", padx=8, font=("Helvetica", 10),
                  command=self._clear_buffers).pack(side="left", padx=4)

        # Start refresh loop
        self._schedule_draw()

    # ------------------------------------------------------------------
    def _setup_ax(self, ax, title: str):
        ax.set_title(title, color=TEXT_COLOR, fontsize=12,
                     fontweight="bold", loc="left", pad=4)
        ax.grid(True, linewidth=0.4, alpha=0.6)
        ax.tick_params(labelsize=7)
        ax.set_xlabel("t (s)", color=TEXT_COLOR, fontsize=10)

    def _on_window_change(self, _=None):
        w = self.window_var.get()
        for key in self.buffers:
            old = list(self.buffers[key])
            self.buffers[key] = deque([0.0] * w, maxlen=w)
            self.buffers[key].extend(old[-w:] if len(old) >= w else old)
        self.timestamps = deque(range(-w, 0), maxlen=w)

    def _clear_buffers(self):
        w = self.window_var.get()
        for key in self.buffers:
            self.buffers[key] = deque([0.0] * w, maxlen=w)
        self.timestamps = deque(range(-w, 0), maxlen=w)

    # ------------------------------------------------------------------
    # Public API: called from notification_handler / sensor parsing
    # ------------------------------------------------------------------
    def push(self, channel: str, value: float):
        if channel in self.buffers and not self.pause_var.get():
            self.buffers[channel].append(value)

    def push_batch(self, data: dict):
        for ch, val in data.items():
            self.push(ch, val)

    # ------------------------------------------------------------------
    def _schedule_draw(self):
        if self._running:
            self._redraw()
            self.after(1500, self._schedule_draw)   # ~10 fps

    def _redraw(self):
        if self.pause_var.get():
            return

        xs = list(range(len(next(iter(self.buffers.values())))))

        for key, ln in self.lines.items():
            ys = list(self.buffers[key])
            ln.set_xdata(xs)
            ln.set_ydata(ys)

        for ax in [self.ax_multi, self.ax_press, self.ax_accel, self.ax_lux]:
            ax.relim()
            ax.autoscale_view()
            ax.set_xlim(0, len(xs) - 1)

        self.canvas_widget.draw_idle()

    def destroy(self):
        self._running = False
        plt.close(self.fig)
        super().destroy()
