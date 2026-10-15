#!/usr/bin/env python3
"""
PC agent for the ESP32-S3 dual-OLED dashboard (Windows 11).

- Hardware stats: LibreHardwareMonitor remote web server (http://localhost:8085/data.json)
  Falls back to psutil for CPU/RAM/battery usage if LHM isn't running (no temps then).
- Now playing: Windows media session (SMTC) -> works with the Spotify desktop app.
- Sends one JSON line per interval over USB serial to the ESP32.

Usage:
    python pc_agent.py                 # auto-detect ESP32 port
    python pc_agent.py --port COM5
    python pc_agent.py --dump          # print every LHM sensor found, then exit
"""
import argparse
import asyncio
import json
import re
import time
from datetime import datetime, timezone

import psutil
import requests
import serial
from serial.tools import list_ports

from winsdk.windows.media.control import (
    GlobalSystemMediaTransportControlsSessionManager as MediaManager,
    GlobalSystemMediaTransportControlsSessionPlaybackStatus as PlaybackStatus,
)

LHM_URL = "http://localhost:8085/data.json"
ICON_KIND = {
    "cpu.png": "cpu",
    "nvidia.png": "gpu",
    "ati.png": "gpu",
    "ram.png": "ram",
    "hdd.png": "disk",
}
GPU_NAME_RE = re.compile(r"nvidia|geforce|radeon|intel.*(uhd|iris|arc)", re.I)
NUM_RE = re.compile(r"-?\d+(?:[.,]\d+)?")


# ----------------------------------------------------------------- stats ---
def parse_value(text):
    m = NUM_RE.search(text or "")
    return float(m.group().replace(",", ".")) if m else None


def classify(node, current):
    icon = (node.get("ImageURL") or "").split("/")[-1].lower()
    if icon in ICON_KIND:
        return ICON_KIND[icon]
    name = node.get("Text", "")
    if GPU_NAME_RE.search(name):
        return "gpu"
    if name.lower() in ("generic memory", "total memory", "memory"):
        return "ram" if node.get("Children") else current
    return current


def walk(node, kind=None, cat=None, out=None):
    out = [] if out is None else out
    kind = classify(node, kind)
    text = node.get("Text", "")
    if text in ("Temperatures", "Load", "Fans", "Data", "Clocks", "Power", "Voltages"):
        cat = text
    children = node.get("Children") or []
    if not children and node.get("Value") and cat:
        out.append((kind, cat, text, parse_value(node["Value"])))
    for c in children:
        walk(c, kind, cat, out)
    return out


def fetch_sensors():
    r = requests.get(LHM_URL, timeout=0.8)
    r.raise_for_status()
    return walk(r.json())


def pick(sensors, kind, cat, names=None, agg=max):
    vals = [
        v for k, c, n, v in sensors
        if k == kind and c == cat and v is not None
        and (names is None or any(x in n for x in names))
    ]
    return round(agg(vals)) if vals else None


def read_stats():
    battery = psutil.sensors_battery()
    stats = {"bat": round(battery.percent) if battery else None}
    try:
        s = fetch_sensors()
        stats.update(
            cpu_t=pick(s, "cpu", "Temperatures", ["Package", "Tdie", "Tctl", "Average"])
            or pick(s, "cpu", "Temperatures"),
            cpu_u=pick(s, "cpu", "Load", ["CPU Total"]),
            gpu_t=pick(s, "gpu", "Temperatures", ["GPU Core"]) or pick(s, "gpu", "Temperatures"),
            gpu_u=pick(s, "gpu", "Load", ["GPU Core"]),
            ram_u=pick(s, "ram", "Load", ["Memory"]),
            ssd_t=pick(s, "disk", "Temperatures"),
        )
    except Exception:
        stats.update(
            cpu_t=None, gpu_t=None, ssd_t=None,
            gpu_u=None,
            cpu_u=round(psutil.cpu_percent()),
            ram_u=round(psutil.virtual_memory().percent),
        )
    return stats


# ------------------------------------------------------------------ song ---
def clean(text, limit=80):
    text = (text or "").replace("\n", " ")
    try:
        from unidecode import unidecode  # optional: transliterates Hindi/CJK etc.
        text = unidecode(text)
    except ImportError:
        pass
    return text.encode("ascii", "replace").decode()[:limit]


async def read_song():
    try:
        mgr = await MediaManager.request_async()
        sess = next(
            (s for s in mgr.get_sessions()
             if "spotify" in s.source_app_user_model_id.lower()),
            None,
        ) or mgr.get_current_session()
        if sess is None:
            return None
        info = await sess.try_get_media_properties_async()
        playing = sess.get_playback_info().playback_status == PlaybackStatus.PLAYING
        tl = sess.get_timeline_properties()
        dur = (tl.end_time - tl.start_time).total_seconds()
        pos = tl.position.total_seconds()
        if playing:
            try:  # SMTC only refreshes position occasionally, so add elapsed time
                pos += (datetime.now(timezone.utc) - tl.last_updated_time).total_seconds()
            except Exception:
                pass
        pos = max(0, min(pos, dur)) if dur > 0 else 0
        return {
            "title": clean(info.title),
            "artist": clean(info.artist),
            "playing": playing,
            "pos": int(pos),
            "dur": int(dur),
        }
    except Exception:
        return None


# ---------------------------------------------------------------- serial ---
def open_serial(port):
    if port is None:
        for p in list_ports.comports():
            if p.vid == 0x303A or "CP210" in (p.description or "") or "CH340" in (p.description or ""):
                port = p.device
                break
    if port is None:
        print("ESP32 not found - plug it in or pass --port COMx")
        return None
    try:
        ser = serial.Serial(port, 115200, timeout=0.1, write_timeout=1)
        print(f"Connected on {port}")
        return ser
    except serial.SerialException as e:
        print(f"Cannot open {port}: {e}")
        return None


async def main(args):
    if args.dump:
        for k, c, n, v in fetch_sensors():
            print(f"{k or '?':5} {c:13} {n:30} {v}")
        return
    ser = None
    while True:
        t0 = time.time()
        if ser is None:
            ser = open_serial(args.port)
        payload = {"s": await asyncio.to_thread(read_stats), "m": await read_song()}
        if ser:
            try:
                ser.write((json.dumps(payload, separators=(",", ":")) + "\n").encode())
            except (serial.SerialException, OSError):
                print("Serial lost, reconnecting...")
                ser.close()
                ser = None
        await asyncio.sleep(max(0, args.interval - (time.time() - t0)))


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", help="COM port, e.g. COM5 (auto-detect if omitted)")
    ap.add_argument("--interval", type=float, default=1.0, help="seconds between updates")
    ap.add_argument("--dump", action="store_true", help="list LibreHardwareMonitor sensors and exit")
    try:
        asyncio.run(main(ap.parse_args()))
    except KeyboardInterrupt:
        pass
