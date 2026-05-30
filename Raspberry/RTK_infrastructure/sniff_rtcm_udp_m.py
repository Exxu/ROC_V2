#!/usr/bin/env python3
import sys
import time
from collections import Counter
from pymavlink import mavutil

def rtcm_type_and_len_from_frame(frame: bytes):
    if len(frame) < 6 or frame[0] != 0xD3:
        return None, None
    payload_len = ((frame[1] & 0x03) << 8) | frame[2]
    if payload_len < 2:
        return None, payload_len
    b0 = frame[3]
    b1 = frame[4]
    msg_type = ((b0 << 4) | (b1 >> 4)) & 0x0FFF
    return msg_type, payload_len

def parse_rtcm_frames(buf: bytearray, counts: Counter):
    frames = 0

    while True:
        idx = buf.find(b"\xD3")
        if idx < 0:
            # acotar buffer si no hay preámbulo
            if len(buf) > 8192:
                del buf[:-1024]
            break

        if len(buf) < idx + 3:
            # no hay header completo
            if idx > 0:
                del buf[:idx]
            break

        payload_len = ((buf[idx+1] & 0x03) << 8) | buf[idx+2]
        frame_len = 3 + payload_len + 3  # header + payload + CRC24Q

        if len(buf) < idx + frame_len:
            # frame incompleto; recorta basura antes del preámbulo
            if idx > 0:
                del buf[:idx]
            break

        frame = bytes(buf[idx:idx+frame_len])
        msg_type, _ = rtcm_type_and_len_from_frame(frame)
        if msg_type is not None:
            counts[msg_type] += 1
            frames += 1

        # consumir hasta el final del frame
        del buf[:idx+frame_len]

    return frames

def main():
    if len(sys.argv) < 2:
        print("Uso: sniff_rtcm_udp_fixed.py <host> <port>")
        print("Ej:  sniff_rtcm_udp_fixed.py 127.0.0.1 14541")
        sys.exit(1)

    host = sys.argv[1]
    port = int(sys.argv[2]) if len(sys.argv) >= 3 else 14541

    m = mavutil.mavlink_connection(f'udpin:{host}:{port}')

    buf = bytearray()

    # stats por segundo
    counts = Counter()
    frames_ok = 0
    bytes_in = 0
    chunks = 0

    last_report = time.time()

    while True:
        msg = m.recv_match(blocking=True, timeout=1.0)
        if msg and msg.get_type() == 'GPS_RTCM_DATA':
            chunks += 1
            data = bytes(msg.data[:msg.len])  # recorta padding a 180
            bytes_in += len(data)
            buf.extend(data)
            frames_ok += parse_rtcm_frames(buf, counts)

        now = time.time()
        if now - last_report >= 1.0:
            dt = now - last_report
            bps = int(bytes_in / dt) if dt > 0 else 0
            fps = int(frames_ok / dt) if dt > 0 else 0
            top = counts.most_common(6)
            top_str = ", ".join([f"{k}:{v}" for k, v in top]) if top else "(sin frames)"
            print(f"[udpin {host}:{port}] {bps} B/s, {fps} frames/s | tipos: {top_str} | chunks:{chunks}")

            # reset
            counts.clear()
            frames_ok = 0
            bytes_in = 0
            chunks = 0
            last_report = now

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nSaliendo.")
