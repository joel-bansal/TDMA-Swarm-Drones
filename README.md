# TDMA-Swarm-Drones

## Custom TDMA-Based Communication for UAV Swarms

This repository contains the work carried out as part of an Undergraduate Project (UGP) at IIT Kanpur, focused on developing and exploring a communication system for decentralised Unmanned Aerial Vehicle (UAV) swarms.

The project investigates the use of **Time Division Multiple Access (TDMA)** for managing channel access among multiple UAV nodes, along with Software Defined Radio (SDR)-based communication using GNU Radio and the **ADALM-Pluto SDR**.

The repository is organised into two main components:

1. **TDMA Gateway:** A custom C-based implementation of the TDMA scheduling and packet-management mechanism.
2. **GNU Radio Utilities:** The SDR communication implementation, including GNU Radio flowgraphs and Python utilities for packet transmission and reception.

Each directory contains its own README with detailed documentation and instructions for setting up and running that component.

---

## 1. Project Objectives

The broader objective is to explore a reliable and scalable communication framework for UAV swarms, where multiple autonomous nodes need to exchange information and coordinate their actions.

The key objectives of the project are:

- Develop a custom TDMA-based channel-access mechanism for multiple UAV nodes.
- Implement periodic frame scheduling and time-slot allocation.
- Manage packet buffering and transmission according to assigned slots.
- Integrate the communication software with GNU Radio.
- Explore SDR-based wireless communication using the ADALM-Pluto SDR.
- Investigate the practical challenges involved in building a communication system for larger UAV swarms.

---

## 2. Repository Structure

```text
TDMA-Swarm-Drones/
│
├── README.md
│
├── TDMA_Gateway/
│   ├── README.md
│   └── [TDMA gateway source files]
│
└── GNU_Radio Utils/
    ├── README.md
    └── [GNU Radio flowgraphs, Python utilities,
        and supporting files]
```

The two directories represent different components of the overall project. Their individual READMEs contain the detailed implementation documentation.

---

## 3. Project Components

### 3.1 TDMA Gateway

**Directory:** `TDMA_Gateway/`

This component contains the custom C-based TDMA gateway implementation.

The gateway acts as the MAC-layer component of the communication system. It interfaces with a Linux TAP network interface, manages incoming packets, schedules their transmission opportunities, and forwards framed packets to GNU Radio through UDP.

The main features include:

- **TDMA scheduling:** Divides time into periodic frames and allocates transmission slots to individual nodes.
- **Packet queueing:** Uses a bounded FIFO queue to buffer packets until their scheduled transmission opportunity.
- **Custom packet framing:** Adds a TDMA header containing node identification, frame and slot information, sequence numbers, transmission timestamps, and CRC information.
- **Linux TAP integration:** Provides an interface for receiving network packets.
- **GNU Radio integration:** Forwards framed packets through UDP for subsequent physical-layer processing.
- **Configurable parameters:** Supports changes to node count, node ID, frame duration, contention period, and guard interval.

For detailed information about the architecture, design decisions, compilation, configuration, and execution, refer to:

**[TDMA Gateway README](TDMA_Gateway/README.md)**


### 3.2 GNU Radio Utilities

**Directory:** `GNU_Radio Utils/`

This component contains the GNU Radio and Python utilities used for SDR-based communication experiments with the ADALM-Pluto SDR.

The implementation explores a BPSK-based wireless communication link and provides the supporting software for transmitting and receiving packet data.

The main components include:

- **GNU Radio flowgraph:** Implements the BPSK transmitter and receiver processing chains.
- **PlutoSDR integration:** Interfaces with the ADALM-Pluto SDR for RF transmission and reception.
- **TAP-to-UDP transmitter:** Captures packets from the Linux TAP interface, adds the required framing information, and forwards them to GNU Radio.
- **Receiver parser:** Processes received data, reconstructs packets, checks sequence and CRC information, and records reception statistics.
- **Logging and analysis:** Maintains packet arrival and error logs to help analyse communication behaviour.

The documented experimental configuration uses a 915 MHz RF link.

For detailed information about hardware requirements, GNU Radio configuration, Python utilities, execution steps, experimental results, and troubleshooting, refer to:

**[GNU Radio Utilities README](GNU_Radio%20Utils/README.md)**

---

## 4. Overall System Perspective

The project explores communication across two complementary layers:

| Component | Primary responsibility |
|---|---|
| TDMA Gateway | MAC-layer scheduling, packet buffering, and TDMA packet framing |
| GNU Radio Utilities | Physical-layer signal processing, SDR interfacing, and RF communication |

The TDMA gateway focuses on controlling when packets are transmitted, while the GNU Radio implementation focuses on processing and transmitting data through the SDR hardware.

Together, these components explore the software and hardware requirements for a flexible communication framework that can be extended towards multi-node UAV swarm applications.

The detailed interfaces, configurations, and execution procedures for each implementation are documented in their respective directories.

---

## 5. Experimental Platform

The project uses the following technologies and hardware:

| Technology / Component | Purpose |
|---|---|
| Linux | Host operating system and TAP interface |
| C | TDMA gateway implementation |
| Python | Packet transmission and reception utilities |
| GNU Radio | Physical-layer signal processing |
| ADALM-Pluto SDR | Software-defined radio hardware |
| BPSK | Digital modulation used in the documented RF implementation |
| UDP | Communication interface between software components |

---

## 6. Getting Started

To explore the project, navigate to the relevant component directory and follow its README.

### TDMA Gateway

```bash
cd "TDMA_Gateway"
```

Refer to `README.md` in this directory for:

- Prerequisites and dependencies.
- TAP interface configuration.
- Compilation instructions.
- TDMA gateway configuration.
- Running the gateway and monitoring statistics.

### GNU Radio Utilities

```bash
cd "GNU_Radio Utils"
```

Refer to `README.md` in this directory for:

- Hardware and software requirements.
- PlutoSDR connection and configuration.
- GNU Radio flowgraph setup.
- Running the transmitter and receiver utilities.
- Generating test traffic and examining results.

**Note:** The two implementations have their own setup and execution procedures. Follow the instructions in the corresponding README when running either component.

---

## 7. Current Scope and Future Work

The project provides an initial foundation for exploring custom TDMA-based communication and SDR integration for UAV swarms.

Potential directions for further development include:

- Distributed clock synchronisation among participating nodes.
- Improved timing accuracy and real-time scheduling.
- Receiver-side TDMA packet parsing and validation.
- Dynamic and demand-aware slot allocation.
- Improved multi-node RF testing and validation.
- Adaptive frame and guard-interval configuration.
- Evaluation of scalability as the number of UAV nodes increases.

These developments would help move towards a more robust and scalable communication infrastructure for decentralised UAV swarms.

## Acknowledgement

This work was carried out as part of an Undergraduate Project at IIT Kanpur, exploring communication mechanisms for decentralised UAV swarm systems.

We sincerely acknowledge the guidance and opportunity provided throughout the project.