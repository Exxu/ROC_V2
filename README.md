# ROC v2 Control and Integration Repository

This repository contains the configuration files, scripts, and embedded firmware used to integrate the robot control system. The project is organized around three main components:

1. the Raspberry Pi companion computer;
2. the Pixhawk autopilot configuration and support scripts;
3. the firmware for the external tools and auxiliary embedded devices.

The repository is intended to support the development, configuration, and deployment of a robotic platform that uses MAVLink communication, Pixhawk-based low-level control, and future ROS-based integration.

---

## System Overview

The system uses a Raspberry Pi as a companion computer connected to a Pixhawk autopilot. The Raspberry Pi is responsible for running communication services, routing MAVLink messages, executing support scripts, and later hosting ROS nodes for higher-level control and system integration.

The Pixhawk is responsible for the real-time autopilot layer, actuator outputs, RC input handling, and low-level vehicle control. Configuration files and scripts related to the Pixhawk are included in this repository to document and reproduce the setup.

External tools or mechanisms connected to the robot are controlled by dedicated embedded firmware. These firmwares are also included in the repository to keep the complete platform configuration in a single place.

---

## Operating System

The Raspberry Pi runs:

```text
Ubuntu Server 22.04.5 LTS (Jammy)
Architecture: arm64
Platform: Raspberry Pi
Execution mode: headless, with SSH access
```

The system does not use a graphical interface. Configuration, execution, monitoring, and debugging are performed through the terminal, usually over SSH.

---

## MAVLink Communication

The platform uses MAVLink as the main communication protocol between the Raspberry Pi and the Pixhawk.

The Raspberry Pi runs `mavlink-router`, which is responsible for routing MAVLink messages between:

- the Pixhawk serial/USB connection;
- local UDP endpoints;
- monitoring tools;
- custom scripts running on the Raspberry Pi.

The main `mavlink-router` configuration file is located at:

```bash
/etc/mavlink-router/main.conf
```

After modifying this file, the service must be restarted with:

```bash
sudo systemctl restart mavlink-router
```

The service logs can be monitored with:

```bash
journalctl -u mavlink-router -f
```

---

## ROS Integration

The repository is also intended to support ROS-based development.

ROS will be used for higher-level software components such as:

- robot state monitoring;
- communication with external tools;
- command interfaces;
- diagnostics;
- integration with navigation or supervision layers;
- communication between the Raspberry Pi and other onboard or offboard systems.

MAVLink remains responsible for the communication with the Pixhawk, while ROS will provide a modular framework for the rest of the robotic software architecture.

---

## Pixhawk Configuration and Scripts

The repository includes configuration files and scripts related to the Pixhawk setup.

These files are used to document and reproduce the autopilot configuration, including parameters, communication settings, actuator behavior, auxiliary channels, and custom scripts when applicable.

The Pixhawk layer is responsible for:

- receiving MAVLink messages from the Raspberry Pi;
- handling RC input;
- controlling actuators and outputs;
- executing configured autopilot behavior;
- interacting with the vehicle hardware through the selected firmware stack.

Keeping the Pixhawk configuration files in the repository makes it easier to track changes, restore known working configurations, and maintain consistency between the onboard computer and the autopilot.

---

## Tool Firmware

The repository also contains firmware for the robot tools and auxiliary embedded devices.

These firmwares are used to control external mechanisms connected to the robotic platform, such as actuated tools, relays, sensors, or custom electronic interfaces.

Depending on the tool, the firmware may be responsible for:

- receiving commands from the Raspberry Pi;
- controlling digital outputs;
- reading sensors;
- reporting tool state;
- detecting faults or abnormal operating conditions;
- implementing low-level safety logic.

This allows each tool to have its own embedded control layer while still being integrated into the general robot architecture through the Raspberry Pi and, later, ROS.

---

## Repository Scope

This repository is not limited to a single script or subsystem. Its purpose is to centralize the configuration and software needed for the complete platform integration.

It includes:

- Raspberry Pi setup files and support scripts;
- MAVLink and `mavlink-router` configuration;
- Pixhawk configuration files and scripts;
- firmware for external tools and embedded controllers;
- future ROS-related packages, launch files, and nodes.

---

## Typical Development Workflow

A typical workflow is:

1. configure the Raspberry Pi system and communication services;
2. configure `mavlink-router` in `/etc/mavlink-router/main.conf`;
3. restart and validate the MAVLink routing service;
4. load or update Pixhawk configuration files and scripts;
5. flash or update the firmware for the external tools;
6. test the MAVLink communication between the Raspberry Pi and Pixhawk;
7. integrate higher-level functionality through ROS.

---

## Notes

This repository should be treated as the central configuration and software reference for the robot platform. Any change in Raspberry Pi services, Pixhawk configuration, MAVLink routing, ROS integration, or tool firmware should be documented and versioned here whenever possible.
