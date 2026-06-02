# hil_communication_standalone_timed

Standalone CMake version of `hil_communication`, without ROS/catkin/ament.

It adds timed serial reception:

- `Serial::readByteTimed(...)`
- `Serial::readBytesTimed(...)`
- `HilSerial::readFrameTimed(...)`

The example `keyboard_tool_controller` sends tool commands from the keyboard and waits for ACK/STATUS with timeout and retries.

## Build

```bash
sudo apt update
sudo apt install -y cmake g++ libboost-system-dev libboost-thread-dev

cd hil_communication_standalone_timed
rm -rf build
mkdir build
cd build
cmake ..
make -j$(nproc)
```

## Run

```bash
./keyboard_tool_controller /dev/ttyAMA0 9600 1 500 3
```

Format:

```bash
./keyboard_tool_controller <serial_port> <baudrate> <device_id> [timeout_ms] [max_attempts]
```
