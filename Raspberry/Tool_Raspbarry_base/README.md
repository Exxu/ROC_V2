# hil_communication_standalone_timed_v4

Standalone CMake project. No ROS, no catkin, no ament.

This package builds the executable:

```bash
keyboard_tool_controller_timed
```

It does **not** build `keyboard_tool_controller`.

At startup, the program must print:

```text
keyboard_tool_controller_timed V4 - HIL readFrameTimed - no ROS
argc=...
```

If you do not see `V4`, you are running an old binary.

## Build

```bash
sudo apt update
sudo apt install -y cmake g++ libboost-system-dev libboost-thread-dev

unzip hil_communication_standalone_timed_v4.zip
cd hil_communication_standalone_timed_v4
rm -rf build
mkdir build
cd build
cmake ..
make -j$(nproc)
```

## Test without parameters

```bash
./keyboard_tool_controller_timed
```

Expected behavior: it prints usage and exits. It must not open the serial port.

## Run

```bash
./keyboard_tool_controller_timed /dev/ttyAMA0 9600 1 500 3
```

Format:

```bash
./keyboard_tool_controller_timed <serial_port> <baudrate> <device_id> [timeout_ms] [max_attempts]
```

Example with short timeout:

```bash
./keyboard_tool_controller_timed /dev/ttyAMA0 9600 1 200 1
```

## Verify you are running this version

```bash
strings ./keyboard_tool_controller_timed | grep V4
```

Expected:

```text
keyboard_tool_controller_timed V4 - HIL readFrameTimed - no ROS
```
