# TDMA for UAV Swarm Communication

## Overview

This repository contains a custom **Time Division Multiple Access (TDMA)** implementation.

The broader objective of this project is to develop a reliable and scalable communication system for a decentralised UAV (Unmanned Aerial Vehicle) swarm.

## Project Objectives

- Develop a custom TDMA-based communication mechanism for multiple UAV nodes.
- Implement time-slot scheduling and packet queuing.
- Integrate the TDMA implementation with GNU Radio.
- Test the communication system using the **ADALM-Pluto SDR**.
- Explore the potential of SDR-based communication as a flexible foundation for future UAV swarm networks.

## Implementation

The TDMA implementation includes:

- **Time-slot scheduling:** Divides communication into frames and assigns transmission slots to individual nodes.
- **Packet management:** Buffers incoming packets and releases them during the respective node's allocated transmission slot.
- **Packet framing:** Adds a custom TDMA header containing node ID, frame ID, slot ID, sequence number, transmission timestamp and CRC information.
- **TAP interface:** Uses a Linux TAP interface for packet-level network integration.
- **GNU Radio integration:** Transfers framed packets to GNU Radio through UDP for physical-layer processing and transmission.

In conjunction with [GNU Radio for ADAM Pluto SDR](https://github.com/joel-bansal/TDMA-Swarm-Drones.git)

## 1. Motivation and Background

A UAV swarm consists of multiple drones that need to communicate and coordinate with one another while operating as a distributed system.

Each drone may need to share information such as:
- Position and navigation data.
- Telemetry and flight status.
- Sensor information.
- Mission updates and coordination messages.

As the number of drones increases, managing access to a shared wireless communication channel becomes increasingly challenging.

A conventional shared channel allows multiple nodes to attempt transmission simultaneously, potentially leading to contention and unpredictable access delays.

**The objective of this project is to explore TDMA as a structured channel-access mechanism for UAV swarm communication.**

In TDMA, communication is divided into periodic frames, and each participating node is assigned a specific time slot in which it is allowed to transmit.

This provides a predictable transmission schedule and a foundation for managing communication among multiple UAV nodes.

## 2. System Architecture

The implementation follows a software-to-SDR communication pipeline:

          Network Traffic
                 |
                 v
          Linux TAP Interface
                 |
                 v
          TDMA Gateway (C)
                 |
          +------+------+
          |             |
     Packet Queue   TDMA Scheduler
          |             |
          +------+------+
                 |
                 v
          TDMA Packet Framing
                 |
                 v
                UDP
                 |
                 v
             GNU Radio
                 |
                 v
          ADALM-Pluto SDR
                 |
                 v
          RF Transmission

The TDMA gateway acts as the interface between network packets and the SDR transmission pipeline.

Its primary responsibilities are:
1. Receive packets from a Linux TAP interface.
2. Buffer packets until their scheduled transmission opportunity.
3. Determine whether the current time falls within the node's allocated slot.
4. Construct a custom TDMA packet header.
5. Forward the framed packet to GNU Radio through UDP.

GNU Radio handles the subsequent physical-layer signal processing and SDR interfacing.

## 3. Design and Implementation

### 3.1 TDMA Frame Structure

Time is divided into periodic frames. Each frame consists of an optional contention region followed by transmission slots assigned to individual nodes.

    |------------------------- TDMA FRAME --------------------------|
    
    | Contention |   Node 1   |   Node 2   |   Node 3   |    ...    |
    |   Period   |    Slot    |    Slot    |    Slot    |           |
    
                  <--- Scheduled transmission opportunities --->

The duration of each transmission slot is calculated as:

$$ T_{\text{slot}} = \frac{T_{\text{frame}}-T_{\text{contention}}}{N} $$

where:

- $$T_{\text{frame}}$$ is the total frame duration.
- $$T_{\text{contention}}$$ is the reserved contention period.
- $$N$$ is the configured number of nodes.

A guard interval is also reserved within each slot to provide additional timing margin.

The active transmission duration is:

$$ T_{\text{active}} = T_{\text{slot}}-T_{\text{guard}} $$

Each node is assigned a unique node ID, which determines its position in the TDMA schedule.

### 3.2 Packet Queuing

Packets arriving through the TAP interface are not transmitted immediately.

Instead, they are stored in a bounded FIFO queue until the node's assigned transmission slot becomes active.

The implementation uses:
- Maximum queue capacity: 256 packets.
- Maximum packet payload: 1400 bytes.
- FIFO packet ordering.
- Packet drop counting when the queue is full or a packet exceeds the supported payload size.

This separates packet arrival time from transmission time and allows the scheduler to control when packets enter the transmission pipeline.

### 3.3 Custom TDMA Packet Header

Each outgoing packet is encapsulated with a custom TDMA header.

The header contains:

| Field | Purpose |
|---|---|
| Magic number | Identifies a TDMA packet |
| Version | Identifies the header format version |
| Type | Identifies the packet type |
| Header length | Specifies header size |
| Frame ID | Identifies the current TDMA frame |
| Slot ID | Identifies the allocated transmission slot |
| Node ID | Identifies the transmitting node |
| Sequence number | Tracks packet ordering |
| Payload length | Specifies payload size |
| Flags | Reserved for additional packet information |
| TX timestamp | Records transmission time |
| Payload CRC32 | Carries a CRC32 checksum of the payload |

The implementation uses packed C structures for the wire-format header and converts multibyte fields to network byte order before transmission.

The CRC32 field provides a mechanism for the receiver to check payload integrity. Successful verification requires corresponding receiver-side CRC checking.

### 3.4 GNU Radio Integration

After constructing the TDMA packet, the gateway adds a GNU Radio-compatible preamble containing:

- Access code.
- First packet-length field.
- Second packet-length field.

The resulting packet is sent through a UDP socket to GNU Radio.

The default configuration uses:
- GNU Radio IP: `127.0.0.1`
- GNU Radio UDP port: `17001`
- Access code: `0xE546BA38`

This approach separates MAC-layer scheduling and packet management from physical-layer signal processing.

### 3.5 Why These Design Decisions?

| Design decision | Reason |
|---|---|
| Linux TAP interface | Provides a network-layer interface for injecting and receiving Ethernet frames. |
| FIFO packet queue | Separates packet arrival from scheduled transmission. |
| Fixed TDMA slots | Provides a simple and predictable initial scheduling mechanism. |
| Guard interval | Provides timing margin between transmission opportunities. |
| Custom packet header | Carries node identity, scheduling information and packet metadata. |
| CRC32 | Adds payload-integrity information to the transmitted packet. |
| UDP interface to GNU Radio | Keeps the scheduler independent of the SDR signal-processing implementation. |
| Configurable parameters | Allows experiments with different node counts and timing configurations. |

The implementation is intentionally modular: the TDMA gateway handles scheduling and framing, while GNU Radio handles the physical-layer processing.

## 4. Configuration Parameters

The gateway supports the following command-line arguments:

| Argument | Description | Default |
|---|---|---|
| `--node-id` | ID of the transmitting node | 1 |
| `--nodes` | Total number of TDMA nodes | 3 |
| `--tap` | Linux TAP interface name | `sdr0` |
| `--gnu-ip` | GNU Radio destination IP | `127.0.0.1` |
| `--gnu-port` | GNU Radio UDP destination port | 17001 |
| `--frame-us` | TDMA frame duration in microseconds | 100000 |
| `--contention-us` | Contention region duration in microseconds | 0 |
| `--guard-us` | Guard interval in microseconds | 6000 |
| `--broadcast` | Use UDP broadcast as the destination | Disabled |

The defaults above reflect the configuration initializer in the current source code.

For example, with three nodes and the default configuration:

$$ T_{\text{slot}} = \frac{100000}{3} \approx 33333\ \mu s $$

With a guard interval of 6000 microseconds:

$$ T_{\text{active}} \approx 27333\ \mu s $$

All timing parameters are expressed in microseconds.

## 5. Building the Project

### Requirements

The current gateway implementation requires:

- Linux operating system.
- GCC compiler.
- Linux TUN/TAP support.
- GNU Radio for the SDR transmission pipeline.
- ADALM-Pluto SDR for RF experiments.

### Compile

Assuming the gateway source file is named `tdma_gateway.c`:

    gcc -O2 -Wall -Wextra \
        -o tdma_gateway tdma_gateway.c

If your source file has a different name, replace `tdma_gateway.c` accordingly.

No external C libraries are explicitly required by the gateway source beyond the standard Linux and C system interfaces.

## 6. Running the TDMA Gateway

### Step 1: Create the TAP Interface

The gateway uses a Linux TAP interface to receive Ethernet frames.

Create the interface:

    sudo ip tuntap add dev sdr0 mode tap

Bring it up:

    sudo ip link set sdr0 up

Verify that the interface exists:

    ip link show sdr0

The gateway opens the TAP interface using `/dev/net/tun` and the Linux `TUNSETIFF` ioctl.

### Step 2: Start the GNU Radio Flowgraph

Configure the GNU Radio flowgraph to receive UDP packets on the configured destination port.

By default, the gateway sends packets to:

    IP Address : 127.0.0.1
    UDP Port   : 17001

The GNU Radio flowgraph must be configured to handle the packet format generated by the gateway, including the preamble and TDMA header.

The corresponding physical-layer flowgraph is responsible for processing and transmitting the data using the SDR.

### Step 3: Run the Gateway

For a three-node TDMA configuration, start node 1 using:

    sudo ./tdma_gateway \
        --node-id 1 \
        --nodes 3 \
        --tap sdr0 \
        --gnu-ip 127.0.0.1 \
        --gnu-port 17001 \
        --frame-us 100000 \
        --contention-us 0 \
        --guard-us 6000

For another node, change the node ID:

    sudo ./tdma_gateway --node-id 2 --nodes 3

The same scheduling parameters must be used across participating nodes.

Each node must have its own appropriate TAP interface and GNU Radio destination configuration.

### Step 4: Monitor Gateway Statistics

The gateway periodically prints transmission statistics to the terminal.

Example format:

    stats unicast_rx=... tx=... queued=... dropped=... frame=...

The fields indicate:

- `unicast_rx`: Accepted unicast packets received from TAP.
- `tx`: Packets successfully handed off through the UDP socket.
- `queued`: Current number of packets waiting for transmission.
- `dropped`: Packets rejected because of payload-size or queue-capacity limits.
- `frame`: Current TDMA frame identifier.

To stop the gateway:

    Ctrl+C

The gateway prints its final statistics before exiting.

## 7. Experimental Setup

The implementation was integrated with:

- **ADALM-Pluto SDR:** Used as the SDR hardware platform.
- **GNU Radio:** Used for physical-layer processing and SDR interfacing.
- **Linux TAP:** Used as the network packet ingress interface.
- **C-based TDMA gateway:** Used for scheduling, buffering and packet framing.

The project investigated the integration of a software-defined TDMA MAC layer with an SDR-based communication system.

The software gateway and the RF transmission pipeline should be considered separate stages of the system: successful UDP handoff does not by itself establish successful over-the-air packet reception.

## 8. Current Limitations and Future Work

The current implementation is a prototype and has several areas for further development.

- **Clock synchronisation:** The scheduler uses the local monotonic clock to determine frame and slot positions. A distributed clock-synchronisation mechanism is not implemented in this gateway.
- **Timing accuracy:** Scheduling is implemented in userspace and is not a hard real-time system.
- **Receiver-side validation:** Complete receiver-side TDMA parsing, sequence validation and CRC verification are required for end-to-end packet integrity.
- **Dynamic scheduling:** The current gateway uses fixed slot allocation. Demand-aware allocation can be explored to improve channel utilisation.
- **Multi-node RF validation:** Further testing is needed to establish reliable operation with multiple independent transmitting and receiving nodes.
- **Adaptive timing:** Guard intervals and frame parameters can be investigated under different channel and network conditions.

These improvements are important steps toward a robust and scalable communication infrastructure for larger UAV swarms.

## 9. Reference Code

The repository also contains an `Initial_Reference/` directory with code from an earlier implementation that was used as a reference during the initial stages of the project.

The primary focus of this repository is the custom TDMA implementation developed as part of the undergraduate project.


## Acknowledgement

This work was carried out as part of an undergraduate project exploring communication mechanisms for decentralised UAV swarm systems.

**Project focus:** Custom TDMA scheduling, packet management and integration with software-defined radio.
