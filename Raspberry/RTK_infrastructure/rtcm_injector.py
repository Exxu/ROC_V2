from pymavlink import mavutil
import serial
import time

# LoRa por UART GPIO
lora = serial.Serial('/dev/ttyAMA0', 38400, timeout=0.1)

# Conectar al router (NO a /dev/ttyACM0)
mav = mavutil.mavlink_connection('udpout:127.0.0.1:14540')

seq = 0

while True:
    data = lora.read(180)
    if not data:
        continue

    mav.mav.gps_rtcm_data_send(
        seq & 0xFF,
        len(data),
        data.ljust(180, b'\0')
    )
    seq += 1
    time.sleep(0.01)
