#!/usr/bin/env python3
"""Check a Raspberry Pi UART for valid NMEA GPS data and a position fix."""

import argparse
import math
import os
import select
import sys
import termios
import time


def parse_nmea_sentence(line):
    line = line.strip()
    if not line.startswith(b"$") or b"*" not in line:
        return None

    body, checksum = line[1:].rsplit(b"*", 1)
    if len(checksum) != 2:
        return None

    try:
        expected = int(checksum, 16)
        fields = body.decode("ascii").split(",")
    except (ValueError, UnicodeDecodeError):
        return None

    actual = 0
    for byte in body:
        actual ^= byte
    if actual != expected:
        return None
    return fields


def parse_coordinate(value, hemisphere, degree_digits, maximum):
    try:
        degrees = int(value[:degree_digits])
        minutes = float(value[degree_digits:])
    except (ValueError, IndexError):
        return None

    if minutes < 0 or minutes >= 60 or degrees > maximum:
        return None
    coordinate = degrees + minutes / 60
    if coordinate > maximum or hemisphere not in ("N", "S", "E", "W"):
        return None
    return -coordinate if hemisphere in ("S", "W") else coordinate


def sentence_fix(fields):
    sentence_type = fields[0][-3:]
    if sentence_type == "GGA" and len(fields) > 6 and fields[6] not in ("", "0"):
        lat = parse_coordinate(fields[2], fields[3], 2, 90)
        lon = parse_coordinate(fields[4], fields[5], 3, 180)
        return (lat, lon) if lat is not None and lon is not None else None
    if sentence_type == "RMC" and len(fields) > 6 and fields[2] == "A":
        lat = parse_coordinate(fields[3], fields[4], 2, 90)
        lon = parse_coordinate(fields[5], fields[6], 3, 180)
        return (lat, lon) if lat is not None and lon is not None else None
    return None


def read_gps(port, seconds):
    fd = os.open(port, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        settings = termios.tcgetattr(fd)
        settings[0] = 0
        settings[1] = 0
        settings[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
        settings[3] = 0
        settings[4] = termios.B9600
        settings[5] = termios.B9600
        settings[6][termios.VMIN] = 0
        settings[6][termios.VTIME] = 0
        termios.tcsetattr(fd, termios.TCSANOW, settings)

        valid_sentences = 0
        invalid_sentences = 0
        fix = None
        buffer = bytearray()
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            ready, _, _ = select.select([fd], [], [], min(0.5, deadline - time.monotonic()))
            if not ready:
                continue
            chunk = os.read(fd, 512)
            if not chunk:
                continue
            buffer.extend(chunk)
            while b"\n" in buffer:
                line, _, remainder = buffer.partition(b"\n")
                buffer = bytearray(remainder)
                if not line.startswith(b"$"):
                    continue
                fields = parse_nmea_sentence(line)
                if fields is None:
                    invalid_sentences += 1
                    continue
                valid_sentences += 1
                fix = sentence_fix(fields) or fix

        return valid_sentences, invalid_sentences, fix
    finally:
        os.close(fd)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/serial0", help="UART device (default: /dev/serial0)")
    parser.add_argument("--seconds", type=float, default=15, help="listen duration (default: 15)")
    args = parser.parse_args()
    if not math.isfinite(args.seconds) or args.seconds <= 0:
        parser.error("--seconds must be a finite number greater than zero")

    print(f"Listening on {args.port} at 9600 baud for {args.seconds:g} seconds...")
    try:
        valid, invalid, fix = read_gps(args.port, args.seconds)
    except OSError as exc:
        print(f"UART unavailable: {exc}", file=sys.stderr)
        print("Enable the Raspberry Pi UART and check that the port and permissions are correct.", file=sys.stderr)
        return 2

    print(f"Valid NMEA sentences: {valid}; invalid NMEA sentences: {invalid}")
    if fix:
        print(f"GPS FIX: YES - latitude {fix[0]:.6f}, longitude {fix[1]:.6f}")
    elif valid:
        print("GPS module is sending valid data, but has no position fix yet. Try outdoors and wait.")
    else:
        print("GPS DATA: NO — check power, common ground, TX/RX wiring, and UART configuration.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
