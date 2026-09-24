######################################################################################
#
# capture.py
#
# Grabs a single still frame from a USB webcam (DirectShow on Windows) and saves it as
# a PNG - used to visually verify the RTK_Everywhere e-paper display's real output
# during hardware-in-the-loop testing (epaper_update.md Section 12).
#
# Use: python capture.py [output.png] [--index N] [--warmup N] [--list]
#
#   --list     list available camera indices (opens each briefly) and exit
#   --index N  which camera to open (default 0)
#   --warmup N frames to read and discard before saving (webcams often need a few
#              frames to settle exposure/focus after opening) - default 10
#
######################################################################################

import argparse
import sys

import cv2


def list_cameras(max_index=8):
    found = []
    for i in range(max_index):
        cap = cv2.VideoCapture(i, cv2.CAP_MSMF)
        if cap.isOpened():
            ok, frame = cap.read()
            if ok:
                h, w = frame.shape[:2]
                found.append((i, w, h))
        cap.release()
    if not found:
        print("No cameras found (tried indices 0.." + str(max_index - 1) + ")")
    for i, w, h in found:
        print(f"index {i}: {w}x{h}")


def capture(out_path, index=0, warmup=10):
    cap = cv2.VideoCapture(index, cv2.CAP_MSMF)
    if not cap.isOpened():
        print(f"ERROR: could not open camera index {index}", file=sys.stderr)
        sys.exit(1)

    # Ask for a decent resolution - the webcam will clamp to its real max if lower.
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1920)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 1080)

    frame = None
    for _ in range(warmup):
        ok, frame = cap.read()
        if not ok:
            print("ERROR: failed to read frame during warmup", file=sys.stderr)
            cap.release()
            sys.exit(1)

    cap.release()
    cv2.imwrite(out_path, frame)
    h, w = frame.shape[:2]
    print(f"Saved {w}x{h} -> {out_path}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("output", nargs="?", default="capture.png")
    parser.add_argument("--index", type=int, default=0)
    parser.add_argument("--warmup", type=int, default=10)
    parser.add_argument("--list", action="store_true")
    args = parser.parse_args()

    if args.list:
        list_cameras()
    else:
        capture(args.output, args.index, args.warmup)
