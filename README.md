# Raspberry Pi – Pixhawk Communication System

## Overview

This repository contains the software components used to configure and operate a Raspberry Pi connected to a Pixhawk-based autopilot system.

The current setup is focused on communication between the Raspberry Pi and the Pixhawk using the MAVLink protocol. The system is intended to support future integration with ROS, allowing the Raspberry Pi to act as an intermediate computer for communication, monitoring, and higher-level robotic functionalities.

## Operating System

The system was tested on:

```text
Ubuntu Server 22.04.5 LTS (Jammy)
Raspberry Pi version
Architecture: arm64
Headless installation with SSH access
```

The Raspberry Pi is operated without a graphical interface. Access to the system is performed through SSH.

## MAVLink Communication

The communication with the Pixhawk is based on the MAVLink protocol.

MAVLink is used to exchange messages between the Raspberry Pi and the autopilot, allowing the Raspberry Pi to send, receive, and route data related to the vehicle system.

In this setup, `mavlink-router` is used to manage MAVLink communication endpoints. It allows MAVLink messages to be routed between serial interfaces and UDP ports, making it possible to connect the Pixhawk with local scripts, monitoring tools, and external applications.

The main MAVLink configuration file is located at:

```bash
/etc/mavlink-router/main.conf
```

After modifying this file, the MAVLink router service must be restarted:

```bash
sudo systemctl restart mavlink-router
```

The service logs can be monitored with:

```bash
journalctl -u mavlink-router -f
```

## ROS Integration

ROS will be used in future stages of the project to integrate the Raspberry Pi communication layer with robotic software components.

The planned ROS integration may include:

- communication between ROS nodes and the Pixhawk;
- monitoring of vehicle state and telemetry;
- publication and subscription of robot-related topics;
- integration with sensors, tools, or higher-level control modules;
- use of the Raspberry Pi as an onboard companion computer.

At the current stage, the repository documents and organizes the base communication layer required before adding the ROS interface.

## Repository Purpose

The purpose of this repository is to provide a structured base for:

- configuring the Raspberry Pi operating system;
- setting up MAVLink communication with the Pixhawk;
- organizing scripts and configuration files;
- preparing the system for future ROS-based development.

## System Role

The Raspberry Pi acts as a companion computer connected to the Pixhawk. Its role is to manage communication, execute auxiliary scripts, and provide a future interface between the autopilot and ROS-based software modules.


