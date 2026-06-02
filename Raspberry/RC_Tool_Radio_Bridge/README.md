# ROC RC Tool Radio Bridge

This project is a standalone C++ test program for commanding the ROC tool from the RC radio through MAVLink.

It is intentionally independent from `Raspberry/Tool_Raspbarry_base`. The existing tool project and firmware are not modified. This bridge only reuses the same RS485/HIL-frame protocol used by the current `Simple_firmware`:

```text
1 = UP
2 = DOWN
3 = STATUS
```

## Objective

Read RC channel 8 from the Pixhawk through the existing `mavlink-router` running on the Raspberry Pi, and send `UP` or `DOWN` commands to the Arduino tool controller over RS485.

## Current ROC port assignment

```text
/dev/ttyUSB0  -> telemetry radio / MAVLink link used by mavlink-router
/dev/ttyAMA0  -> RS485 module connected to the Arduino tool bus
```

This program does **not** open `/dev/ttyUSB0`. That port must remain under the control of `mavlink-router`.

The bridge listens to the MAVLink UDP endpoint already exposed by the router:

```text
mavlink-router -> UDP 127.0.0.1:14542 -> roc_rc_tool_radio_bridge
```

and sends tool commands through:

```text
roc_rc_tool_radio_bridge -> /dev/ttyAMA0 -> RS485 -> Arduino Simple_firmware
```

## Control logic

The program is configured for a **two-position switch** on **RC channel 8**.

Default mapping:

```text
PWM < 1500 us  -> DOWN
PWM > 1500 us  -> UP
```

A hysteresis band of `80 us` is applied by default to avoid false transitions caused by PWM noise.

The program sends commands only when the switch position changes:

```text
DOWN -> UP    sends UP once
UP -> DOWN    sends DOWN once
UP -> UP      sends nothing
DOWN -> DOWN  sends nothing
```

At startup, the program always sends one `UP` command to place the tool in a safe initial state. After that, it captures the current RC8 switch position as the baseline and does not send another command until the switch changes position.

This means that if the physical switch starts in the DOWN position, the program still sends `UP` once at startup, then waits for a real switch transition before sending any other command.

## Build

On the Raspberry Pi:

```bash
cd ~/ROC_V2/Raspberry/RC_Tool_Radio_Bridge

sudo apt update
sudo apt install -y cmake g++

rm -rf build
mkdir build
cd build
cmake ..
make -j$(nproc)
```

The executable will be:

```text
build/roc_rc_tool_radio_bridge
```

## Test without moving the tool

Use `--dry-run` first. This validates the MAVLink/RC side without opening `/dev/ttyAMA0`.

```bash
cd ~/ROC_V2/Raspberry/RC_Tool_Radio_Bridge/build

./roc_rc_tool_radio_bridge --dry-run
```

Expected behavior:

```text
[startup] Sending tool UP command once
[tool] TX command=UP device=1
[dry-run] command not sent to RS485

[rc] baseline captured: ch8=1000 us -> DOWN. No DOWN/UP edge command is sent for the initial switch position.
[rc] switch changed: DOWN -> UP at 2000 us
[tool] TX command=UP device=1
[dry-run] command not sent to RS485
[rc] switch changed: UP -> DOWN at 1000 us
[tool] TX command=DOWN device=1
[dry-run] command not sent to RS485
```

## Run with the real RS485 tool

```bash
cd ~/ROC_V2/Raspberry/RC_Tool_Radio_Bridge/build

./roc_rc_tool_radio_bridge \
  --tool-port /dev/ttyAMA0 \
  --tool-baud 9600 \
  --tool-id 1
```

Because these are the default values, this shorter command is equivalent:

```bash
./roc_rc_tool_radio_bridge
```

## If the switch direction is reversed

If your transmitter produces high PWM when the switch is physically in the DOWN position, run:

```bash
./roc_rc_tool_radio_bridge --invert-switch
```

With this option:

```text
low PWM   -> UP
high PWM  -> DOWN
```

## MAVLink-router assumptions

The repository already uses `mavlink-router` to link the Raspberry Pi and the Pixhawk. The bridge assumes that the router exposes these UDP endpoints:

```ini
[UdpEndpoint in]
Mode=Server
Address=127.0.0.1
Port=14540

[UdpEndpoint monitor]
Mode=Normal
Address=127.0.0.1
Port=14542
```

The bridge listens on `14542` and periodically sends `MAV_CMD_SET_MESSAGE_INTERVAL` to `14540` asking ArduPilot to publish `RC_CHANNELS` at 10 Hz.

If RC messages are already being published and you do not want the bridge to request the rate, use:

```bash
./roc_rc_tool_radio_bridge --no-request-rc-stream
```

## Useful options

```bash
./roc_rc_tool_radio_bridge --help
```

Important options:

```text
--dry-run
--tool-port /dev/ttyAMA0
--tool-baud 9600
--tool-id 1
--rc-channel 8
--switch-threshold 1500
--switch-hysteresis 80
--invert-switch
--status-after-command
```

## Recommended first test sequence

1. Start the Pixhawk and confirm that `mavlink-router` is running.
2. Run the bridge in dry-run mode:

   ```bash
   ./roc_rc_tool_radio_bridge --dry-run
   ```

3. Move the RC8 switch and confirm that only transitions generate commands.
4. Stop the dry-run test.
5. Connect the RS485 bus and run:

   ```bash
   ./roc_rc_tool_radio_bridge --tool-port /dev/ttyAMA0
   ```

6. Confirm that startup sends `UP` once.
7. Move the switch and confirm that each position change sends only one command.

## Future ROS integration

This standalone project is intentionally structured as a small C++ module to make the next step easier. A ROS 2 architecture can reuse the same separation:

```text
MAVLink RC reader       -> reads RC_CHANNELS from mavlink-router
Tool RS485 driver       -> sends UP/DOWN/STATUS to Simple_firmware
High-level supervisor   -> decides tool actions from RC/manual/autonomous logic
```

In ROS 2, this can become:

```text
roc_rc_input_node
roc_tool_driver_node
roc_tool_supervisor_node
```
