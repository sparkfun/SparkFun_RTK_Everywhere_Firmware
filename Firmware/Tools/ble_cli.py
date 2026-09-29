######################################################################################
#
# ble_cli.py
#
# Talk to an RTK device's CLI over its BLE command service from a laptop, the way a
# phone app does. Requires: pip install bleak
#
# Interactive: type commands with or without the $...*checksum framing, for example
#   SPGET,subsystemVersions
#   SPEXE,UPDATEAP
#
#   python ble_cli.py
#
# Local firmware update test (OTA_Local.ino): starts the update soft AP, waits while
# you join it from the laptop's Wi-Fi menu, serves the files over HTTP, then queues,
# starts and follows the update. One --file per subsystem, as subsystem:chip:path.
#
#   python ble_cli.py update --file GNSS:LG290P:C:\fw\LG290P03AANR01A06S.pkg
#   python ble_cli.py update --file ESP32:ESP32:RTK_Everywhere_Firmware.bin
#
# Windows asks whether Python may accept connections the first time the HTTP server
# starts: allow it on Public networks, since the device's soft AP is an unidentified
# (Public) network.
#
######################################################################################

import argparse
import asyncio
import os
import re
import socket
import sys
import threading
import time
import zlib
from functools import reduce
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

from bleak import BleakClient, BleakScanner

COMMAND_SERVICE_UUID = "7e400001-b5a3-f393-e0a9-e50e24dcca9e"  # Bluetooth.ino
COMMAND_RX_UUID = "7e400002-b5a3-f393-e0a9-e50e24dcca9e"  # Device receives (we write)
COMMAND_TX_UUID = "7e400003-b5a3-f393-e0a9-e50e24dcca9e"  # Device sends (notifications)

# The main serial (SPP-over-BLE) service. A legacy advertisement packet is only 31 bytes,
# too small to fit both this and the command service UUID alongside the device name, so a
# device may advertise only one of the two even though both services exist once connected.
SERIAL_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"  # Bluetooth.ino


def frame(command):
    """Add the $ and *checksum framing unless the command already has it."""
    command = command.strip()
    if command.startswith("$"):
        return command
    checksum = reduce(lambda a, c: a ^ ord(c), command, 0)
    return f"${command}*{checksum:02X}"


class RtkBle:
    def __init__(self, client):
        self.client = client
        self.rx_buffer = ""
        self.lines = asyncio.Queue()
        self.quiet = False

    def _notify(self, _sender, data):
        self.rx_buffer += data.decode("latin-1")
        while "\n" in self.rx_buffer:
            line, self.rx_buffer = self.rx_buffer.split("\n", 1)
            line = line.strip()
            if line:
                self.lines.put_nowait(line)

    async def start(self):
        await self.client.start_notify(COMMAND_TX_UUID, self._notify)
        char = self.client.services.get_characteristic(COMMAND_RX_UUID)
        self.no_response = "write-without-response" in char.properties
        mtu = getattr(self.client, "mtu_size", 23) or 23
        self.chunk = max(20, mtu - 3)

    async def send(self, command):
        data = (frame(command) + "\r\n").encode("latin-1")
        for i in range(0, len(data), self.chunk):
            await self.client.write_gatt_char(COMMAND_RX_UUID, data[i : i + self.chunk], response=not self.no_response)

    async def query(self, command, timeout=5.0):
        """Send a command and return the first response line for it ($SPGET/$SPEXE/$SP...)."""
        while not self.lines.empty():
            self.lines.get_nowait()
        await self.send(command)
        prefix = "$" + frame(command)[1:6]  # $SPGET, $SPEXE, ...
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return None
            try:
                line = await asyncio.wait_for(self.lines.get(), remaining)
            except asyncio.TimeoutError:
                return None
            if line.startswith(prefix) or line.startswith("$SP,"):
                return line


async def find_device(name, address):
    if address:
        return address
    print("Scanning for RTK devices...")
    # Don't filter the scan itself by service UUID: the command service may not fit in the
    # advertisement (see SERIAL_SERVICE_UUID above), so filter in Python on whatever the
    # device actually advertised (either known service UUID, or just its name).
    devices = await BleakScanner.discover(timeout=8.0, return_adv=True)

    def advertises_rtk_service(adv):
        uuids = [u.lower() for u in adv.service_uuids]
        return COMMAND_SERVICE_UUID in uuids or SERIAL_SERVICE_UUID in uuids

    def name_matches(device):
        return name is None or (device.name and name.lower() in device.name.lower())

    matches = [device for device, adv in devices.values() if advertises_rtk_service(adv) and name_matches(device)]
    if not matches and name is not None:
        # Neither RTK service UUID fit in this device's advertisement; fall back to matching
        # on name alone since --name narrows things down enough to make that safe.
        matches = [device for device, _adv in devices.values() if device.name and name_matches(device)]
    if not matches:
        sys.exit("No device found. Is Bluetooth set to BLE (or SPP + BLE) and no phone connected? "
                  "Try --name to filter by device name, or --address if you know the device's BLE address.")
    for i, device in enumerate(matches):
        print(f"  {i}) {device.name} [{device.address}]")
    if len(matches) == 1:
        return matches[0].address
    choice = input("Device number: ")
    return matches[int(choice)].address


async def interactive(rtk):
    print("Type CLI commands (Ctrl+C to quit). Framing and checksum are added for you.")

    async def printer():
        while True:
            print("<", await rtk.lines.get())

    task = asyncio.create_task(printer())
    loop = asyncio.get_running_loop()
    try:
        while True:
            command = await loop.run_in_executor(None, input, "> ")
            if command.strip():
                print(">", frame(command))
                await rtk.send(command)
            await asyncio.sleep(0.3)
    finally:
        task.cancel()


def local_ipv4_addresses():
    try:
        return {a[4][0] for a in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET)}
    except socket.gaierror:
        return set()


def start_http_server(files, port):
    """Serve files[n] at /n/<basename>. Returns the server."""

    class Handler(BaseHTTPRequestHandler):
        def _file(self):
            match = re.match(r"^/(\d+)/", self.path)
            if match and int(match.group(1)) < len(files):
                return files[int(match.group(1))]
            return None

        def _headers(self, path):
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(os.path.getsize(path)))
            self.end_headers()

        def do_HEAD(self):
            path = self._file()
            if path is None:
                self.send_error(404)
                return
            self._headers(path)

        def do_GET(self):
            path = self._file()
            if path is None:
                self.send_error(404)
                return
            self._headers(path)
            sent = 0
            start = time.monotonic()
            with open(path, "rb") as f:
                while chunk := f.read(16 * 1024):
                    try:
                        self.wfile.write(chunk)
                    except (BrokenPipeError, ConnectionResetError):
                        print(f"\nHTTP: device closed the connection after {sent} bytes")
                        return
                    sent += len(chunk)
            print(f"\nHTTP: sent {os.path.basename(path)}, {sent} bytes in {time.monotonic() - start:.1f} s")

        def log_message(self, fmt, *args):
            print(f"\nHTTP: {self.client_address[0]} {fmt % args}")

    server = ThreadingHTTPServer(("0.0.0.0", port), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


async def update(rtk, file_specs, port, laptop_ip):
    # Parse and check the files
    files = []
    for spec in file_specs:
        parts = spec.split(":", 2)
        if len(parts) != 3:
            sys.exit(f"--file {spec}: use subsystem:chip:path")
        subsystem, chip, path = parts
        if not os.path.isfile(path):
            sys.exit(f"--file {spec}: {path} not found")
        with open(path, "rb") as f:
            crc = zlib.crc32(f.read()) & 0xFFFFFFFF
        files.append((subsystem, chip, path, os.path.getsize(path), crc))

    print("Versions:", await rtk.query("SPGET,subsystemVersions"))

    before = local_ipv4_addresses()
    response = await rtk.query("SPEXE,UPDATEAP")
    print(response)
    match = re.search(r'UPDATEAP,"([^"]*)","([^"]*)",OK', response or "")
    if not match:
        sys.exit("UPDATEAP failed")
    ssid, password = match.groups()

    print("Waiting for the soft AP...")
    for _ in range(40):
        status = await rtk.query("SPGET,updateStatus")
        if status and "AP_READY" in status:
            break
        await asyncio.sleep(1)
    else:
        sys.exit(f"Soft AP did not start: {status}")

    print()
    print(f"  Join this Wi-Fi network from the laptop:  {ssid}")
    print(f"  Password:                                 {password}")
    print()

    # Keep polling while waiting so the soft AP's idle timeout does not expire
    loop = asyncio.get_running_loop()
    waiting = loop.run_in_executor(None, input, "Press Enter once the laptop is connected...")
    while not waiting.done():
        await rtk.query("SPGET,updateStatus")
        await asyncio.wait([waiting], timeout=10)

    if laptop_ip is None:
        new = local_ipv4_addresses() - before
        if len(new) == 1:
            laptop_ip = new.pop()
        else:
            laptop_ip = input(f"Laptop IP address on {ssid} (see ipconfig): ").strip()
    print(f"Laptop IP: {laptop_ip}")

    server = start_http_server([f[2] for f in files], port)
    try:
        for index, (subsystem, chip, path, size, crc) in enumerate(files):
            url = f"http://{laptop_ip}:{port}/{index}/{os.path.basename(path)}"
            command = f"SPEXE,UPDATEFILE,{subsystem},{chip},{url},{size},0x{crc:08X}"
            print(">", command)
            response = await rtk.query(command)
            print("<", response)
            if not response or ",OK*" not in response:
                await rtk.query("SPEXE,UPDATECANCEL")
                sys.exit("UPDATEFILE failed, soft AP stopped")

        response = await rtk.query("SPEXE,UPDATESTART")
        print("<", response)
        if not response or ",OK*" not in response:
            await rtk.query("SPEXE,UPDATECANCEL")
            sys.exit("UPDATESTART failed, soft AP stopped")

        # Follow the update until it completes (and the device reboots) or fails
        last = None
        while True:
            try:
                status = await rtk.query("SPGET,updateStatus", timeout=5)
            except Exception:
                status = None
            if status is None:
                if not rtk.client.is_connected:
                    print("\nBLE disconnected: the device is rebooting." if last and "COMPLETE" in last
                          else "\nBLE disconnected.")
                    break
                continue
            if status != last:
                value = status.split('"')[1] if '"' in status else status
                print(f"\r{value:60}", end="", flush=True)
                last = status
            if "FAILED" in status:
                print("\nUpdate failed. Retry with SPEXE,UPDATESTART or stop with SPEXE,UPDATECANCEL.")
                break
            await asyncio.sleep(2)
    finally:
        server.shutdown()


async def main():
    parser = argparse.ArgumentParser(description="RTK CLI over BLE")
    parser.add_argument("--name", help="Part of the device's Bluetooth name")
    parser.add_argument("--address", help="Device BLE address (skips the scan)")
    sub = parser.add_subparsers(dest="mode")
    up = sub.add_parser("update", help="Local firmware update test")
    up.add_argument("--file", action="append", required=True, help="subsystem:chip:path, e.g. GNSS:LG290P:fw.pkg")
    up.add_argument("--port", type=int, default=8080, help="HTTP server port (default 8080)")
    up.add_argument("--ip", help="Laptop IP address on the soft AP (found automatically if omitted)")
    args = parser.parse_args()

    address = await find_device(args.name, args.address)
    print(f"Connecting to {address}...")
    # use_cached_services=False: Windows caches a device's GATT table by BLE address, and the
    # RTK device's services can change across firmware flashes without the address changing,
    # which otherwise makes connect() hang until timeout trying to reconcile a stale cache.
    async with BleakClient(address, timeout=20.0, winrt=dict(use_cached_services=False)) as client:
        rtk = RtkBle(client)
        await rtk.start()
        print("Connected")
        if args.mode == "update":
            await update(rtk, args.file, args.port, args.ip)
        else:
            await interactive(rtk)


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
