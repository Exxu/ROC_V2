#!/usr/bin/env python3
import sys
import time
from collections import Counter
import serial

def rtcm_type_and_len(frame: bytes):
    """
    frame incluye: 0xD3 + 2 bytes len + payload + 3 bytes CRC
    Devuelve (msg_type, payload_len)
    """
    if len(frame) < 6 or frame[0] != 0xD3:
        return None, None
    payload_len = ((frame[1] & 0x03) << 8) | frame[2]
    # msg type son los 12 bits altos de los dos primeros bytes del payload
    # payload empieza en frame[3]
    if payload_len < 2:
        return None, payload_len
    b0 = frame[3]
    b1 = frame[4]
    msg_type = ((b0 << 4) | (b1 >> 4)) & 0x0FFF
    return msg_type, payload_len

def main():
    if len(sys.argv) < 3:
        print("Uso: sniff_rtcm_serial.py <device> <baud>")
        print("Ej:  sniff_rtcm_serial.py /dev/ttyAMA1 57600")
        sys.exit(1)

    dev = sys.argv[1]
    baud = int(sys.argv[2])

    ser = serial.Serial(dev, baudrate=baud, timeout=0.1)

    buf = bytearray()
    counts = Counter()
    frames_ok = 0
    bytes_in = 0

    t0 = time.time()
    last_report = t0

    while True:
        chunk = ser.read(4096)
        if chunk:
            buf.extend(chunk)
            bytes_in += len(chunk)

        # Parse RTCM3 frames: D3 + len(10 bits) + payload + CRC24Q(3 bytes)
        i = 0
        while True:
            # buscar preámbulo
            idx = buf.find(b"\xD3", i)
            if idx < 0:
                # mantener pequeño buffer
                if len(buf) > 8192:
                    del buf[:-1024]
                break

            # necesitamos 3 bytes mínimo (D3 + len2)
            if len(buf) < idx + 3:
                # esperar más datos
                if idx > 0:
                    del buf[:idx]
                break

            payload_len = ((buf[idx+1] & 0x03) << 8) | buf[idx+2]
            frame_len = 3 + payload_len + 3  # header + payload + crc

            if len(buf) < idx + frame_len:
                # no hay frame completo todavía
                if idx > 0:
                    del buf[:idx]
                break

            frame = bytes(buf[idx:idx+frame_len])

            # Nota: no calculamos CRC (coste); validamos por estructura.
            msg_type, plen = rtcm_type_and_len(frame)
            if msg_type is not None:
                counts[msg_type] += 1
                frames_ok += 1

            # consumir hasta fin del frame
            del buf[:idx+frame_len]
            i = 0

        now = time.time()
        if now - last_report >= 1.0:
            dt = now - last_report
            bps = int(bytes_in / dt)
            fps = int(frames_ok / dt)

            # top 6 tipos
            top = counts.most_common(6)
            top_str = ", ".join([f"{k}:{v}" for k, v in top]) if top else "(sin frames)"

            print(f"[{dev} @ {baud}] {bps} B/s, {fps} frames/s | tipos: {top_str}")

            # reset contadores por segundo
            bytes_in = 0
            frames_ok = 0
            counts.clear()
            last_report = now

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nSaliendo.")
