# RTCM Injection to Pixhawk Using Raspberry Pi, LoRa, and MAVLink Router

This repository contains the scripts used on the Raspberry Pi to receive RTCM corrections from a serial link, encapsulate them in MAVLink, and send them to a Pixhawk through `mavlink-router`.

The main system flow is:

```text
GNSS Base / LoRa RTCM
        ↓ UART GPIO
/dev/ttyAMA0 @ 38400 baud
        ↓
rtcm_injector.py
        ↓ MAVLink GPS_RTCM_DATA over UDP
127.0.0.1:14540
        ↓
mavlink-router
        ↓ UART/USB
/dev/ttyACM0 @ 115200 baud
        ↓
Pixhawk
```

## Files Covered

This README documents the following files:

| File | Function |
|---|---|
| `rtcm_injector.py` | Main script. Reads RTCM from the LoRa module through UART and sends it as MAVLink `GPS_RTCM_DATA` messages to `mavlink-router`. |
| `sniff_rtcm_serial.py` | Diagnostic tool. Checks whether valid RTCM frames are arriving through a serial port. |
| `sniff_rtcm_udp_m.py` | Diagnostic tool. Checks whether MAVLink `GPS_RTCM_DATA` messages are circulating through UDP and extracts the RTCM message types. |
| `/etc/mavlink-router/main.conf` | System configuration file used by `mavlink-router` to connect the Pixhawk, the RTCM injector, the UDP monitor, and Mission Planner. |

`sniff_rtcm_udp.py` is not considered here because it is a simpler/older version of the UDP monitor and is not required for the flow described in this README.

## Dependencies and Installation

The Raspberry Pi requires:

- Python 3;
- `pyserial`;
- `pymavlink`;
- `mavlink-router`, installed from source.

In this setup, `mavlink-router` was **not** installed with:

```bash
sudo apt install mavlink-router
```

Instead, the `mavlink-router` repository was cloned, compiled, and installed on the Raspberry Pi.

### Python dependencies

Recommended option using a Python virtual environment:

```bash
python3 -m venv venv
source venv/bin/activate
pip install pyserial pymavlink
```

If the scripts are executed without a virtual environment, install the Python packages in the environment being used by the Raspberry Pi:

```bash
pip install pyserial pymavlink
```

### Installing `mavlink-router` from source

Install the build dependencies first. The exact package list can vary depending on the Raspberry Pi OS version, but the typical requirements are `git`, `meson`, `ninja`, and compiler/build tools:

```bash
sudo apt update
sudo apt install git meson ninja-build pkg-config gcc g++
```

Clone the repository:

```bash
git clone https://github.com/mavlink-router/mavlink-router.git
cd mavlink-router
```

Initialize the submodules:

```bash
git submodule update --init --recursive
```

Configure and compile:

```bash
meson setup build .
ninja -C build
```

Install it system-wide:

```bash
sudo ninja -C build install
```

After installation, verify that `mavlink-routerd` is available:

```bash
which mavlink-routerd
mavlink-routerd --help
```

If the command is not found after installation, check whether it was installed under `/usr/local/bin` and confirm that this directory is included in the system `PATH`.

After installing from source, the active configuration file used in this setup is:

```bash
/etc/mavlink-router/main.conf
```

Edit it with:

```bash
sudo nano /etc/mavlink-router/main.conf
```

## `mavlink-router` Configuration

In this setup, the active `mavlink-router` configuration is edited directly in the system path:

```bash
sudo nano /etc/mavlink-router/main.conf
```

The file `/etc/mavlink-router/main.conf` defines the main connections:

```ini
[General]
ReportStats=true

[UartEndpoint px4]
Device=/dev/ttyACM0
Baud=115200

[UdpEndpoint in]
Mode=Server
Address=127.0.0.1
Port=14540

[UdpEndpoint monitor]
Mode=Normal
Address=127.0.0.1
Port=14542

[UdpEndpoint missionplanner]
Mode=Normal
Address=10.140.27.96
Port=14550
```

Meaning of each endpoint:

| Endpoint | Function |
|---|---|
| `UartEndpoint px4` | Physical connection to the Pixhawk through `/dev/ttyACM0` at `115200` baud. |
| `UdpEndpoint in` | Local UDP port where `rtcm_injector.py` sends MAVLink `GPS_RTCM_DATA` messages. |
| `UdpEndpoint monitor` | Local UDP output used to monitor the MAVLink traffic forwarded by the router. |
| `UdpEndpoint missionplanner` | UDP output to Mission Planner at the configured IP address. |

Run `mavlink-router` manually with the system configuration file:

```bash
mavlink-routerd -c /etc/mavlink-router/main.conf
```

If `mavlink-router` was configured as a system service, restart it after editing the configuration:

```bash
sudo systemctl restart mavlink-router
```

Then check its status:

```bash
systemctl status mavlink-router
```

## Main Script: `rtcm_injector.py`

This is the script that must remain running during normal system operation.

The script performs three tasks:

1. Opens the serial port where RTCM arrives from the LoRa module:

   ```python
   lora = serial.Serial('/dev/ttyAMA0', 38400, timeout=0.1)
   ```

2. Connects to `mavlink-router` using UDP:

   ```python
   mav = mavutil.mavlink_connection('udpout:127.0.0.1:14540')
   ```

3. Reads blocks of up to 180 bytes and sends them as MAVLink `GPS_RTCM_DATA` messages:

   ```python
   data = lora.read(180)

   mav.mav.gps_rtcm_data_send(
       seq & 0xFF,
       len(data),
       data.ljust(180, b'\0')
   )
   ```

The Pixhawk does not receive data directly from this script. The script sends the messages to UDP port `127.0.0.1:14540`; then `mavlink-router` forwards them to the Pixhawk through `/dev/ttyACM0`.

### Running the Injector

First start `mavlink-router` using the system configuration:

```bash
mavlink-routerd -c /etc/mavlink-router/main.conf
```

Alternatively, if it is managed as a service:

```bash
sudo systemctl restart mavlink-router
```

Then, in another terminal, start the RTCM injection:

```bash
python3 rtcm_injector.py
```

## Serial Diagnostics: `sniff_rtcm_serial.py`

This script is used to check whether RTCM corrections are arriving correctly through the serial port before injecting them into the Pixhawk.

Usage:

```bash
python3 sniff_rtcm_serial.py <device> <baud>
```

Example using the same port used by `rtcm_injector.py`:

```bash
python3 sniff_rtcm_serial.py /dev/ttyAMA0 38400
```

The script:

- opens the selected serial port;
- reads raw bytes;
- searches for RTCM3 frames starting with `0xD3`;
- computes the frame length;
- identifies the RTCM message type;
- prints bytes per second, frames per second, and detected RTCM types.

Expected output:

```text
[/dev/ttyAMA0 @ 38400] 520 B/s, 4 frames/s | types: 1005:1, 1077:2, 1087:1
```

Interpretation:

| Output | Meaning |
|---|---|
| `B/s` | Number of RTCM bytes received per second. |
| `frames/s` | Number of complete RTCM frames detected per second. |
| `types` | RTCM types detected and how many times they appeared during the last second. |

Important: it is not recommended to run `sniff_rtcm_serial.py` and `rtcm_injector.py` at the same time on the same `/dev/ttyAMA0` port, because both processes would try to read from the same serial stream.

## UDP/MAVLink Diagnostics: `sniff_rtcm_udp_m.py`

This script is used to check whether RTCM corrections are already circulating inside the MAVLink stream as `GPS_RTCM_DATA` messages.

Usage:

```bash
python3 sniff_rtcm_udp_m.py <host> <port>
```

With the current `mavlink-router` configuration, the monitor endpoint is located at `127.0.0.1:14542`. Therefore, the recommended command is:

```bash
python3 sniff_rtcm_udp_m.py 127.0.0.1 14542
```

The script:

- opens an incoming MAVLink UDP connection;
- listens for `GPS_RTCM_DATA` messages;
- extracts `msg.data[:msg.len]` to remove padding up to 180 bytes;
- reconstructs RTCM3 frames;
- searches for the `0xD3` preamble;
- identifies the RTCM message types;
- prints per-second statistics.

Expected output:

```text
[udpin 127.0.0.1:14542] 520 B/s, 4 frames/s | types: 1005:1, 1077:2, 1087:1 | chunks:5
```

Interpretation:

| Field | Meaning |
|---|---|
| `B/s` | RTCM bytes extracted from MAVLink messages per second. |
| `frames/s` | Complete RTCM frames detected per second. |
| `types` | RTCM types found inside the MAVLink stream. |
| `chunks` | Number of MAVLink `GPS_RTCM_DATA` messages received during the last second. |

## Recommended Test Sequence

### 1. Check that the Pixhawk appears on the Raspberry Pi

```bash
ls /dev/ttyACM*
```

You should see something like:

```text
/dev/ttyACM0
```

### 2. Check that the LoRa/RTCM base appears on the expected UART

The main script uses:

```text
/dev/ttyAMA0 @ 38400 baud
```

The port can be checked with:

```bash
ls /dev/ttyAMA*
```

### 3. Test whether RTCM is arriving through serial

```bash
python3 sniff_rtcm_serial.py /dev/ttyAMA0 38400
```

If RTCM types appear, the serial/LoRa link is delivering correction data.

Then stop the sniffer with:

```bash
Ctrl + C
```

### 4. Start `mavlink-router`

Manual execution:

```bash
mavlink-routerd -c /etc/mavlink-router/main.conf
```

Or, if using the system service:

```bash
sudo systemctl restart mavlink-router
systemctl status mavlink-router
```

### 5. Start the RTCM injector

In another terminal:

```bash
python3 rtcm_injector.py
```

### 6. Check that RTCM is leaving through MAVLink/UDP

In another terminal:

```bash
python3 sniff_rtcm_udp_m.py 127.0.0.1 14542
```

If RTCM types appear at this stage, the RTCM data is being encapsulated into MAVLink and forwarded by `mavlink-router`.

## Normal Operation Sequence

To use the system normally, run:

Terminal 1, if running manually:

```bash
mavlink-routerd -c /etc/mavlink-router/main.conf
```

Or start/restart the service:

```bash
sudo systemctl restart mavlink-router
```

Terminal 2:

```bash
python3 rtcm_injector.py
```

The monitors `sniff_rtcm_serial.py` and `sniff_rtcm_udp_m.py` are used only for diagnostics.

## Quick Troubleshooting

### No data arrives in `sniff_rtcm_serial.py`

Possible causes:

- The LoRa/GNSS base is not transmitting RTCM.
- The port is not `/dev/ttyAMA0`.
- The baud rate is not `38400`.
- The Raspberry Pi UART is not enabled.
- Another process is reading from the same serial port.

Check which processes are using the port:

```bash
sudo lsof /dev/ttyAMA0
```

### RTCM arrives through serial, but nothing appears in `sniff_rtcm_udp_m.py`

Possible causes:

- `rtcm_injector.py` is not running.
- `mavlink-router` is not running.
- The UDP input port does not match.
- The injector sends to `127.0.0.1:14540`, but `mavlink-router` is not listening on that port.
- The monitor must listen on `127.0.0.1:14542` according to the current configuration.

Check that `mavlink-router` is active:

```bash
ps aux | grep mavlink-router
```

Check UDP ports:

```bash
ss -lunp | grep 1454
```

### Chunks appear, but no RTCM types appear

Possible causes:

- `GPS_RTCM_DATA` messages are arriving, but their content does not contain complete RTCM frames.
- Bytes are being lost in the link.
- The RTCM stream is fragmented or noisy.
- The RTCM source is not sending standard RTCM3 messages.

In this case, test the serial level again first:

```bash
python3 sniff_rtcm_serial.py /dev/ttyAMA0 38400
```

### Mission Planner does not receive data

The current configuration sends MAVLink to:

```text
10.140.27.96:14550
```

If the computer running Mission Planner has another IP address, change the block:

```ini
[UdpEndpoint missionplanner]
Mode=Normal
Address=10.140.27.96
Port=14550
```

to the correct computer IP address.

## Useful Command Summary

```bash
# Test raw RTCM from LoRa/UART
python3 sniff_rtcm_serial.py /dev/ttyAMA0 38400

# Edit the active MAVLink Router configuration
sudo nano /etc/mavlink-router/main.conf

# Run MAVLink Router manually
mavlink-routerd -c /etc/mavlink-router/main.conf

# Or restart it if running as a service
sudo systemctl restart mavlink-router

# Run the RTCM injector to the Pixhawk
python3 rtcm_injector.py

# Check RTCM inside MAVLink/UDP
python3 sniff_rtcm_udp_m.py 127.0.0.1 14542

# List serial ports
ls /dev/ttyAMA* /dev/ttyACM* /dev/ttyUSB* 2>/dev/null

# Show active related UDP ports
ss -lunp | grep 1454

# Show which process is using the LoRa UART
sudo lsof /dev/ttyAMA0
```

## Note About Serial Permissions

If a permission error appears when opening `/dev/ttyAMA0` or `/dev/ttyACM0`, add the user to the `dialout` group:

```bash
sudo usermod -a -G dialout $USER
```

Then log out and log back in, or reboot the Raspberry Pi.

## Mental Model for Remembering the System

The main idea is:

```text
sniff_rtcm_serial.py  → checks whether RTCM exists on the wire/UART
rtcm_injector.py     → takes that RTCM and sends it to MAVLink Router
mavlink-router       → forwards MAVLink to the Pixhawk
sniff_rtcm_udp_m.py  → checks whether RTCM is already inside the MAVLink stream
```
