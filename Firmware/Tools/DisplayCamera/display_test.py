######################################################################################
#
# display_test.py
#
# Drives an RTK Everywhere unit with a 184x88 e-paper display through its display
# test scenarios (SPEXE,DISPLAYTEST,<n> - see RTK_Everywhere/DisplayTest.ino) over USB
# serial, and photographs the panel after each one with the USB camera (capture.py).
#
# Use:
#   python display_test.py --port COM6                 every scenario
#   python display_test.py --port COM6 --scenarios 1 5 19
#   python display_test.py --port COM6 --list          print the unit's scenario list
#   python display_test.py --port COM6 --off           return the unit to its normal display
#
# Photos go to output/<run timestamp>/<nn>.png.
#
######################################################################################

import argparse
import datetime
import sys
import time
from pathlib import Path

import cv2
import serial

HERE = Path(__file__).parent
MENU_MARKER = b"x) Exit\r\n"  # Last line of the main menu
COMMAND_MARKER = b"COMMAND MODE"


def read_until_any(ser, markers, timeout):
    """Read serial output until any of `markers` appears. Returns (marker or None, data)."""
    data = b""
    end = time.time() + timeout
    while time.time() < end:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            data += chunk
            for marker in markers:
                if marker in data:
                    return marker, data
    return None, data


def enter_command_mode(ser):
    """Get the unit into command mode from whatever state it is in: running, sitting in
    the main menu, or already in command mode."""
    # "x<CR>" steps out one level: command mode -> main menu -> running. From running, the
    # 'x' opens the menu and the bare CR closes it again, so three always ends up running
    for _ in range(3):
        ser.write(b"x\r")
        time.sleep(1.0)
    ser.reset_input_buffer()

    # While running, any single character opens the main menu (and is consumed). With GNSS
    # data forwarded to USB it takes "+++" instead
    ser.write(b"+")
    found, _ = read_until_any(ser, [MENU_MARKER], 3)
    if found is None:
        ser.write(b"++")
        found, _ = read_until_any(ser, [MENU_MARKER], 3)
        if found is None:
            raise RuntimeError("Could not open the main menu")

    time.sleep(0.5)  # Let the menu start reading input - earlier bytes can be dropped
    ser.write(b"+\r")
    found, _ = read_until_any(ser, [COMMAND_MARKER], 3)
    if found is None:
        raise RuntimeError("Could not enter command mode")
    time.sleep(0.5)  # Same again for the command line


def display_test(ser, argument):
    """Send SPEXE,DISPLAYTEST,<argument>. Returns the unit's response text."""
    enter_command_mode(ser)
    # CR only - a trailing LF is read as a second, empty command line and keeps the unit
    # in command mode instead of returning to its normal display
    ser.write(f"SPEXE,DISPLAYTEST,{argument}\r".encode())
    _, data = read_until_any(ser, [b",OK*", b"ERROR"], 3)
    time.sleep(0.5)
    data += ser.read(ser.in_waiting)
    if str(argument).upper() == "LIST":
        # LIST stays in command mode - leave it so the unit returns to its normal display
        ser.write(b"x\r")
        time.sleep(0.5)
        ser.reset_input_buffer()
    return data.decode(errors="replace")


def capture(path, camera_index=0, warmup=10):
    cap = cv2.VideoCapture(camera_index, cv2.CAP_MSMF)
    if not cap.isOpened():
        raise RuntimeError(f"Could not open camera {camera_index} (is another app using it?)")
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1920)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 1080)
    frame = None
    for _ in range(warmup):
        ok, latest = cap.read()
        if ok:
            frame = latest
        else:
            time.sleep(0.2)
    cap.release()
    if frame is None:
        raise RuntimeError("Camera returned no frames")
    cv2.imwrite(str(path), frame)


def open_port(port):
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = 115200
    ser.timeout = 0.1
    # Keep DTR/RTS low so opening the port doesn't reset the ESP32
    ser.dtr = False
    ser.rts = False
    ser.open()
    return ser


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM6")
    parser.add_argument("--camera", type=int, default=0)
    parser.add_argument("--scenarios", type=int, nargs="*")
    parser.add_argument("--count", type=int, default=28, help="number of scenarios on the unit")
    parser.add_argument("--settle", type=float, default=6.0, help="seconds to wait for the e-paper refresh")
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--off", action="store_true")
    args = parser.parse_args()

    ser = open_port(args.port)
    try:
        if args.list:
            print(display_test(ser, "LIST"))
            return
        if args.off:
            print(display_test(ser, 0))
            return

        out_dir = HERE / "output" / datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        out_dir.mkdir(parents=True, exist_ok=True)

        scenarios = args.scenarios or list(range(1, args.count + 1))
        for n in scenarios:
            response = display_test(ser, n)
            status = next((line for line in response.splitlines() if line.startswith("DISPLAYTEST")),
                          "(no response)")
            print(f"{n:2d}: {status}")
            time.sleep(args.settle)
            path = out_dir / f"{n:02d}.png"
            capture(path, args.camera)
            print(f"    -> {path}")

        display_test(ser, 0)
        print(f"Done. Display test off. Photos in {out_dir}")
    finally:
        ser.close()


if __name__ == "__main__":
    sys.exit(main())
